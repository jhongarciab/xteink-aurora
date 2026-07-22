#pragma once

#include <string>

#define XAURORA_LOG_DIR "/.crosspoint/xaurora-logs"

namespace XAuroraLogs {

const char* getLogDir();

void appendEvent(const char* category, const char* message);
void appendEvent(const char* category, const std::string& message);

bool writeReport(const char* prefix, const std::string& body, std::string* outPath = nullptr);

}  // namespace XAuroraLogs

#ifdef CPR_DISABLE_EVENT_LOGS
#define XAURORA_LOG_EVENT(category, message) \
  do {                                          \
  } while (false)
#define XAURORA_WRITE_REPORT(prefix, body, outPath) false
#else
#define XAURORA_LOG_EVENT(category, message) XAuroraLogs::appendEvent(category, message)
#define XAURORA_WRITE_REPORT(prefix, body, outPath) XAuroraLogs::writeReport(prefix, body, outPath)
#endif
