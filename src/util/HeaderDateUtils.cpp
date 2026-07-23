#include "HeaderDateUtils.h"

#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <ctime>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "ReadingStatsStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TimeUtils.h"

namespace {
void drawHeaderTopLine(const GfxRenderer& renderer, const ThemeMetrics& metrics, const int pageWidth,
                       const std::string& dateText, const std::string& timeText, const std::string& reminderText) {
  const bool showBatteryPercentage =
      SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS;
  int reminderX = metrics.contentSidePadding + metrics.batteryWidth + 12;

  if (showBatteryPercentage) {
    const std::string batteryText = std::to_string(powerManager.getBatteryPercentage()) + "%";
    reminderX += renderer.getTextWidth(SMALL_FONT_ID, batteryText.c_str()) + 4;
  }

  int dateX = pageWidth - metrics.contentSidePadding;
  if (!dateText.empty()) {
    const int dateWidth = renderer.getTextWidth(SMALL_FONT_ID, dateText.c_str());
    dateX = std::max(reminderX, pageWidth - metrics.contentSidePadding - dateWidth);
    renderer.drawText(SMALL_FONT_ID, dateX, metrics.topPadding + 5, dateText.c_str());
  }

  if (!timeText.empty()) {
    const int timeWidth = renderer.getTextWidth(SMALL_FONT_ID, timeText.c_str());
    renderer.drawText(SMALL_FONT_ID, (pageWidth - timeWidth) / 2, metrics.topPadding + 5, timeText.c_str());
  }

  if (!reminderText.empty()) {
    const int maxReminderWidth = std::max(0, dateX - reminderX - 12);
    if (maxReminderWidth > 0) {
      const std::string truncated = renderer.truncatedText(SMALL_FONT_ID, reminderText.c_str(), maxReminderWidth);
      renderer.drawText(SMALL_FONT_ID, reminderX, metrics.topPadding + 5, truncated.c_str());
    }
  }
}

std::string formatHeaderDateText(const uint32_t timestamp, const bool usedFallback) {
  const bool networkTimeIsCurrent =
      !usedFallback && TimeUtils::isClockValid(timestamp) && APP_STATE.lastNetworkTimeSyncDayOrdinal != 0 &&
      TimeUtils::getLocalDayOrdinal(timestamp) == APP_STATE.lastNetworkTimeSyncDayOrdinal;
  return TimeUtils::formatDate(timestamp, false);
}

std::string formatHeaderTimeText(const uint32_t timestamp, const bool usedFallback) {
  const bool networkTimeIsCurrent =
      !usedFallback && TimeUtils::isClockValid(timestamp) && APP_STATE.lastNetworkTimeSyncDayOrdinal != 0 &&
      TimeUtils::getLocalDayOrdinal(timestamp) == APP_STATE.lastNetworkTimeSyncDayOrdinal;
  if (!networkTimeIsCurrent) return "";
  time_t currentTime = static_cast<time_t>(timestamp);
  tm localTime = {};
  if (localtime_r(&currentTime, &localTime) == nullptr) return "";
  return (localTime.tm_hour < 10 ? "0" : "") + std::to_string(localTime.tm_hour) + ":" +
         (localTime.tm_min < 10 ? "0" : "") + std::to_string(localTime.tm_min);
}
}  // namespace

HeaderDateUtils::DisplayDateInfo HeaderDateUtils::getDisplayDateInfo() {
  TimeUtils::configureTimezone();
  const uint32_t authoritativeTimestamp = TimeUtils::getAuthoritativeTimestamp();
  if (TimeUtils::isClockValid(authoritativeTimestamp)) {
    return {authoritativeTimestamp, false};
  }

  if (TimeUtils::isClockValid(APP_STATE.lastKnownValidTimestamp)) {
    return {APP_STATE.lastKnownValidTimestamp, true};
  }

  bool usedFallback = false;
  const uint32_t timestamp = READING_STATS.getDisplayTimestamp(&usedFallback);
  return {timestamp, usedFallback};
}

std::string HeaderDateUtils::getDisplayDateText() {
  if (!SETTINGS.displayDay) {
    return "";
  }

  const auto info = getDisplayDateInfo();
  return formatHeaderDateText(info.timestamp, info.usedFallback);
}

std::string HeaderDateUtils::getSyncDayReminderText() {
  return "";
}

void HeaderDateUtils::drawTopLine(GfxRenderer& renderer, const std::string& dateText) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const auto info = getDisplayDateInfo();
  drawHeaderTopLine(renderer, metrics, pageWidth, dateText, formatHeaderTimeText(info.timestamp, info.usedFallback),
                    getSyncDayReminderText());
}

void HeaderDateUtils::drawHeaderWithDate(GfxRenderer& renderer, const char* title, const char* subtitle) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title, subtitle);
  const auto info = getDisplayDateInfo();
  drawHeaderTopLine(renderer, metrics, pageWidth, getDisplayDateText(),
                    formatHeaderTimeText(info.timestamp, info.usedFallback), getSyncDayReminderText());
}
