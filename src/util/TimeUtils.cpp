#include "TimeUtils.h"

#include "CrossPointSettings.h"

#include <Arduino.h>
#include <HalGPIO.h>
#include <Logging.h>
#include <Wire.h>
#include <esp_sntp.h>

#include <algorithm>
#include <ctime>
#include <sys/time.h>

#include "util/TimeZoneRegistry.h"

namespace {
constexpr uint32_t VALID_CLOCK_THRESHOLD = 1704067200UL;  // 2024-01-01 UTC
bool syncedThisBoot = false;
uint8_t configuredTimeZonePreset = UINT8_MAX;

uint8_t fromBcd(const uint8_t value) { return static_cast<uint8_t>((value >> 4) * 10U + (value & 0x0FU)); }

uint8_t toBcd(const uint8_t value) { return static_cast<uint8_t>(((value / 10U) << 4) | (value % 10U)); }

bool readRtcRegisters(const uint8_t firstRegister, uint8_t* values, const uint8_t count) {
  Wire.beginTransmission(I2C_ADDR_DS3231);
  Wire.write(firstRegister);
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom(I2C_ADDR_DS3231, count, static_cast<uint8_t>(true)) != count) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (uint8_t i = 0; i < count; ++i) values[i] = Wire.read();
  return true;
}

bool writeRtcRegisters(const uint8_t firstRegister, const uint8_t* values, const uint8_t count) {
  Wire.beginTransmission(I2C_ADDR_DS3231);
  Wire.write(firstRegister);
  for (uint8_t i = 0; i < count; ++i) Wire.write(values[i]);
  return Wire.endTransmission() == 0;
}

bool isLeapYear(const int year) { return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0); }

unsigned getDaysInMonth(const int year, const unsigned month) {
  static constexpr unsigned DAYS_PER_MONTH[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2) {
    return isLeapYear(year) ? 29U : 28U;
  }
  if (month < 1 || month > 12) {
    return 0;
  }
  return DAYS_PER_MONTH[month - 1];
}

std::string formatDateBuffer(const int year, const unsigned month, const unsigned day, const bool appendBang) {
  const char* bang = appendBang ? "!" : "";
  char buffer[24];

  switch (static_cast<CrossPointSettings::DATE_FORMAT>(SETTINGS.dateFormat)) {
    case CrossPointSettings::DATE_MM_DD_YYYY:
      snprintf(buffer, sizeof(buffer), "%02u/%02u/%04d%s", month, day, year, bang);
      break;
    case CrossPointSettings::DATE_YYYY_MM_DD:
      snprintf(buffer, sizeof(buffer), "%04d-%02u-%02u%s", year, month, day, bang);
      break;
    case CrossPointSettings::DATE_DD_MM_YYYY:
    default:
      snprintf(buffer, sizeof(buffer), "%02u/%02u/%04d%s", day, month, year, bang);
      break;
  }

  return buffer;
}

int32_t daysFromCivil(int year, const unsigned month, const unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + static_cast<int>(dayOfEra) - 719468;
}

void civilFromDays(int z, int& year, unsigned& month, unsigned& day) {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned dayOfEra = static_cast<unsigned>(z - era * 146097);
  const unsigned yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  year = static_cast<int>(yearOfEra) + era * 400;
  const unsigned dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const unsigned monthPart = (5 * dayOfYear + 2) / 153;
  day = dayOfYear - (153 * monthPart + 2) / 5 + 1;
  month = monthPart + (monthPart < 10 ? 3 : -9);
  year += (month <= 2);
}
}  // namespace

void TimeUtils::configureTimezone() {
  const uint8_t preset = TimeZoneRegistry::clampPresetIndex(SETTINGS.timeZonePreset);
  if (configuredTimeZonePreset == preset) {
    return;
  }

  setenv("TZ", TimeZoneRegistry::getPresetPosixTz(preset), 1);
  tzset();
  configuredTimeZonePreset = preset;
}

void TimeUtils::stopNtp() {
  if (esp_sntp_enabled()) {
    esp_sntp_stop();
  }
}

bool TimeUtils::syncTimeWithNtp(const uint32_t timeoutMs) {
  configureTimezone();
  stopNtp();

  const bool initialClockValid = isClockValid(static_cast<uint32_t>(time(nullptr)));
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_setservername(1, "time.nist.gov");
  esp_sntp_init();

  const uint32_t maxRetries = std::max<uint32_t>(1, timeoutMs / 100U);
  for (uint32_t retry = 0; retry < maxRetries; ++retry) {
    const time_t currentTime = time(nullptr);
    const bool currentClockValid = isClockValid(static_cast<uint32_t>(currentTime));
    const bool syncCompleted = sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED;
    const bool clockJumpedToValid = !initialClockValid && currentClockValid;

    if ((syncCompleted || clockJumpedToValid) && currentClockValid) {
      syncedThisBoot = true;
      updateHardwareRtcFromSystemTime();
      return true;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  return false;
}

bool TimeUtils::restoreTimeFromHardwareRtc() {
  if (!gpio.deviceIsX3()) return false;

  uint8_t status = 0;
  if (!readRtcRegisters(0x0F, &status, 1) || (status & 0x80U) != 0) {
    LOG_DBG("TIME", "X3 RTC is unavailable or reports oscillator stop");
    return false;
  }

  uint8_t regs[7] = {};
  if (!readRtcRegisters(DS3231_SEC_REG, regs, sizeof(regs))) return false;

  const unsigned second = fromBcd(regs[0] & 0x7FU);
  const unsigned minute = fromBcd(regs[1] & 0x7FU);
  unsigned hour = 0;
  if ((regs[2] & 0x40U) != 0) {
    hour = fromBcd(regs[2] & 0x1FU) % 12U;
    if ((regs[2] & 0x20U) != 0) hour += 12U;
  } else {
    hour = fromBcd(regs[2] & 0x3FU);
  }
  const unsigned day = fromBcd(regs[4] & 0x3FU);
  const unsigned month = fromBcd(regs[5] & 0x1FU);
  const int year = 2000 + fromBcd(regs[6]) + ((regs[5] & 0x80U) != 0 ? 100 : 0);
  if (second > 59 || minute > 59 || hour > 23 || month < 1 || month > 12 || day < 1 ||
      day > getDaysInMonth(year, month)) {
    LOG_DBG("TIME", "X3 RTC returned an invalid date");
    return false;
  }

  const int64_t epoch = static_cast<int64_t>(daysFromCivil(year, month, day)) * 86400LL +
                        static_cast<int64_t>(hour * 3600U + minute * 60U + second);
  if (epoch < 0 || epoch > UINT32_MAX || !isClockValid(static_cast<uint32_t>(epoch))) return false;

  timeval tv{};
  tv.tv_sec = static_cast<time_t>(epoch);
  if (settimeofday(&tv, nullptr) != 0) return false;
  syncedThisBoot = true;
  LOG_DBG("TIME", "System clock restored from X3 RTC");
  return true;
}

bool TimeUtils::updateHardwareRtcFromSystemTime() {
  if (!gpio.deviceIsX3()) return false;

  const time_t now = time(nullptr);
  if (now < 0 || !isClockValid(static_cast<uint32_t>(now))) return false;
  tm utc = {};
  if (gmtime_r(&now, &utc) == nullptr || utc.tm_year + 1900 < 2000 || utc.tm_year + 1900 > 2199) return false;

  const int fullYear = utc.tm_year + 1900;
  const uint8_t regs[7] = {
      toBcd(static_cast<uint8_t>(utc.tm_sec)),
      toBcd(static_cast<uint8_t>(utc.tm_min)),
      toBcd(static_cast<uint8_t>(utc.tm_hour)),
      toBcd(static_cast<uint8_t>(utc.tm_wday == 0 ? 7 : utc.tm_wday)),
      toBcd(static_cast<uint8_t>(utc.tm_mday)),
      static_cast<uint8_t>(toBcd(static_cast<uint8_t>(utc.tm_mon + 1)) | (fullYear >= 2100 ? 0x80U : 0U)),
      toBcd(static_cast<uint8_t>(fullYear % 100)),
  };
  if (!writeRtcRegisters(DS3231_SEC_REG, regs, sizeof(regs))) return false;

  uint8_t status = 0;
  if (readRtcRegisters(0x0F, &status, 1)) {
    status &= static_cast<uint8_t>(~0x80U);
    writeRtcRegisters(0x0F, &status, 1);
  }
  return true;
}

bool TimeUtils::isClockValid() { return isClockValid(static_cast<uint32_t>(time(nullptr))); }

bool TimeUtils::isClockValid(const uint32_t epochSeconds) { return epochSeconds >= VALID_CLOCK_THRESHOLD; }

uint32_t TimeUtils::getAuthoritativeTimestamp() {
  const uint32_t now = static_cast<uint32_t>(time(nullptr));
  if (syncedThisBoot && isClockValid(now)) {
    return now;
  }
  return 0;
}

uint32_t TimeUtils::getCurrentValidTimestamp() {
  const uint32_t now = static_cast<uint32_t>(time(nullptr));
  return isClockValid(now) ? now : 0;
}

bool TimeUtils::setCurrentDate(const int year, const unsigned month, const unsigned day, uint32_t* epochSeconds) {
  configureTimezone();

  const unsigned daysInMonth = getDaysInMonth(year, month);
  if (day < 1 || daysInMonth == 0 || day > daysInMonth) {
    return false;
  }

  tm localTime = {};
  localTime.tm_year = year - 1900;
  localTime.tm_mon = static_cast<int>(month) - 1;
  localTime.tm_mday = static_cast<int>(day);
  localTime.tm_hour = 12;
  localTime.tm_min = 0;
  localTime.tm_sec = 0;
  localTime.tm_isdst = -1;

  const time_t epoch = mktime(&localTime);
  if (epoch < 0 || !isClockValid(static_cast<uint32_t>(epoch))) {
    return false;
  }

  timeval tv{};
  tv.tv_sec = epoch;
  if (settimeofday(&tv, nullptr) != 0) {
    return false;
  }

  syncedThisBoot = true;
  updateHardwareRtcFromSystemTime();
  if (epochSeconds) {
    *epochSeconds = static_cast<uint32_t>(epoch);
  }
  return true;
}

bool TimeUtils::getTimestampForLocalDate(const int year, const unsigned month, const unsigned day,
                                         uint32_t* epochSeconds) {
  configureTimezone();

  const unsigned daysInMonth = getDaysInMonth(year, month);
  if (day < 1 || daysInMonth == 0 || day > daysInMonth) {
    return false;
  }

  tm localTime = {};
  localTime.tm_year = year - 1900;
  localTime.tm_mon = static_cast<int>(month) - 1;
  localTime.tm_mday = static_cast<int>(day);
  localTime.tm_hour = 12;
  localTime.tm_min = 0;
  localTime.tm_sec = 0;
  localTime.tm_isdst = -1;

  const time_t epoch = mktime(&localTime);
  if (epoch < 0 || !isClockValid(static_cast<uint32_t>(epoch))) {
    return false;
  }

  if (epochSeconds) {
    *epochSeconds = static_cast<uint32_t>(epoch);
  }
  return true;
}

uint32_t TimeUtils::getLocalDayOrdinal(const uint32_t epochSeconds) {
  configureTimezone();

  if (!isClockValid(epochSeconds)) {
    return 0;
  }

  time_t currentTime = static_cast<time_t>(epochSeconds);
  tm localTime = {};
  if (localtime_r(&currentTime, &localTime) == nullptr) {
    return epochSeconds / 86400UL;
  }

  return static_cast<uint32_t>(
      daysFromCivil(localTime.tm_year + 1900, static_cast<unsigned>(localTime.tm_mon + 1),
                    static_cast<unsigned>(localTime.tm_mday)));
}

uint32_t TimeUtils::getDayOrdinalForDate(const int year, const unsigned month, const unsigned day) {
  return static_cast<uint32_t>(daysFromCivil(year, month, day));
}

bool TimeUtils::getDateFromDayOrdinal(const uint32_t dayOrdinal, int& year, unsigned& month, unsigned& day) {
  civilFromDays(static_cast<int>(dayOrdinal), year, month, day);
  return true;
}

bool TimeUtils::wasTimeSyncedThisBoot() { return syncedThisBoot; }

const char* TimeUtils::getCurrentTimeZoneLabel() {
  return TimeZoneRegistry::getPresetLabel(TimeZoneRegistry::clampPresetIndex(SETTINGS.timeZonePreset));
}

std::string TimeUtils::formatDate(const uint32_t epochSeconds, const bool appendBang) {
  if (!isClockValid(epochSeconds)) {
    return "";
  }

  configureTimezone();
  time_t currentTime = static_cast<time_t>(epochSeconds);
  tm localTime = {};
  if (localtime_r(&currentTime, &localTime) == nullptr) {
    return "";
  }

  return formatDateBuffer(localTime.tm_year + 1900, static_cast<unsigned>(localTime.tm_mon + 1),
                          static_cast<unsigned>(localTime.tm_mday), appendBang);
}

std::string TimeUtils::formatDateTime(const uint32_t epochSeconds, const bool appendBang) {
  if (!isClockValid(epochSeconds)) {
    return "";
  }

  configureTimezone();
  time_t currentTime = static_cast<time_t>(epochSeconds);
  tm localTime = {};
  if (localtime_r(&currentTime, &localTime) == nullptr) {
    return "";
  }

  return formatDateBuffer(localTime.tm_year + 1900, static_cast<unsigned>(localTime.tm_mon + 1),
                          static_cast<unsigned>(localTime.tm_mday), appendBang) +
         " " + (localTime.tm_hour < 10 ? "0" : "") + std::to_string(localTime.tm_hour) + ":" +
         (localTime.tm_min < 10 ? "0" : "") + std::to_string(localTime.tm_min);
}

std::string TimeUtils::formatDateParts(const int year, const unsigned month, const unsigned day, const bool appendBang) {
  return formatDateBuffer(year, month, day, appendBang);
}

std::string TimeUtils::formatMonthYear(const int year, const unsigned month) {
  char buffer[16];
  if (static_cast<CrossPointSettings::DATE_FORMAT>(SETTINGS.dateFormat) == CrossPointSettings::DATE_YYYY_MM_DD) {
    snprintf(buffer, sizeof(buffer), "%04d-%02u", year, month);
  } else {
    snprintf(buffer, sizeof(buffer), "%02u/%04d", month, year);
  }
  return buffer;
}
