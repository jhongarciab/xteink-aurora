#pragma once

// Version symbols are defined in version.generated.inc (included by version.cpp)
// so dev-counter bumps recompile only that translation unit.

extern const char CPR_CROSSPOINT_VERSION[];
#define CROSSPOINT_VERSION CPR_CROSSPOINT_VERSION

extern const char XAURORA_BASE_VERSION[];

extern const int XAURORA_BUILD_SEQ;

extern const int XAURORA_RELEASE_SEQ;

extern const char XAURORA_BUILD_KIND[];
