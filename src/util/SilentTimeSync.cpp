#include "SilentTimeSync.h"

#include <Arduino.h>
#include <Logging.h>
#include <WiFi.h>

#include <string>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "WifiCredentialStore.h"
#include "util/TimeUtils.h"

namespace {
constexpr uint32_t SCAN_TIMEOUT_MS = 3000;
constexpr uint32_t CONNECT_TIMEOUT_MS = 4500;
constexpr uint32_t NTP_TIMEOUT_MS = 3000;

struct Candidate {
  const WifiCredential* credential = nullptr;
  int32_t rssi = INT32_MIN;
  int32_t channel = 0;
  uint8_t bssid[6] = {};
  bool hasBssid = false;
};

void shutDownWifi() {
  TimeUtils::stopNtp();
  WiFi.scanDelete();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
}

bool waitForScan() {
  WiFi.scanNetworks(true);
  const uint32_t started = millis();
  while (millis() - started < SCAN_TIMEOUT_MS) {
    const int16_t result = WiFi.scanComplete();
    if (result != WIFI_SCAN_RUNNING) return result >= 0;
    delay(50);
  }
  return false;
}

bool selectCandidate(Candidate& selected) {
  const int16_t count = WiFi.scanComplete();
  if (count <= 0) return false;

  const std::string& preferredSsid = WIFI_STORE.getLastConnectedSsid();
  bool selectedIsPreferred = false;
  for (int16_t i = 0; i < count; ++i) {
    const String scannedSsid = WiFi.SSID(i);
    if (scannedSsid.isEmpty()) continue;
    const WifiCredential* credential = WIFI_STORE.findCredential(scannedSsid.c_str());
    if (credential == nullptr) continue;

    const bool isPreferred = !preferredSsid.empty() && credential->ssid == preferredSsid;
    const int32_t rssi = WiFi.RSSI(i);
    if (selected.credential != nullptr && !isPreferred && (selectedIsPreferred || rssi <= selected.rssi)) continue;

    selected.credential = credential;
    selected.rssi = rssi;
    selected.channel = WiFi.channel(i);
    selected.hasBssid = selected.channel > 0 && WiFi.BSSID(i, selected.bssid);
    selectedIsPreferred = isPreferred;
  }
  return selected.credential != nullptr;
}
}  // namespace

bool SilentTimeSync::run(const bool allowWifiAttempt) {
  TimeUtils::restoreTimeFromHardwareRtc();

  const uint32_t currentTimestamp = TimeUtils::getCurrentValidTimestamp();
  const uint32_t currentDay = TimeUtils::getLocalDayOrdinal(currentTimestamp);
  if (currentDay != 0 && currentDay == APP_STATE.lastNetworkTimeSyncDayOrdinal) {
    LOG_DBG("TIME", "Silent WiFi sync skipped: already synchronized today");
    return false;
  }

  if (!allowWifiAttempt || !SETTINGS.autoSyncDay || !WIFI_STORE.loadFromFile() ||
      WIFI_STORE.getCredentials().empty()) {
    return false;
  }

  LOG_DBG("TIME", "Starting silent saved-network time sync");
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect();
  delay(50);

  Candidate candidate;
  if (!waitForScan() || !selectCandidate(candidate)) {
    LOG_DBG("TIME", "Silent time sync found no saved network in range");
    shutDownWifi();
    return false;
  }

  const char* password = candidate.credential->password.empty() ? nullptr : candidate.credential->password.c_str();
  if (candidate.hasBssid) {
    WiFi.begin(candidate.credential->ssid.c_str(), password, candidate.channel, candidate.bssid);
  } else if (password != nullptr) {
    WiFi.begin(candidate.credential->ssid.c_str(), password);
  } else {
    WiFi.begin(candidate.credential->ssid.c_str());
  }

  const uint32_t connectStarted = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - connectStarted < CONNECT_TIMEOUT_MS) delay(50);
  if (WiFi.status() != WL_CONNECTED) {
    LOG_DBG("TIME", "Silent connection to saved network timed out");
    shutDownWifi();
    return false;
  }

  const bool synchronized = TimeUtils::syncTimeWithNtp(NTP_TIMEOUT_MS);
  const uint32_t syncedTimestamp = TimeUtils::getCurrentValidTimestamp();
  if (synchronized && syncedTimestamp != 0) {
    APP_STATE.registerValidTimeSync(syncedTimestamp);
    APP_STATE.lastNetworkTimeSyncDayOrdinal = TimeUtils::getLocalDayOrdinal(syncedTimestamp);
    APP_STATE.saveToFile();
    WIFI_STORE.setLastConnectedSsid(candidate.credential->ssid);
    LOG_DBG("TIME", "Silent time sync completed");
  } else {
    LOG_DBG("TIME", "Silent NTP sync timed out");
  }

  shutDownWifi();
  return synchronized;
}
