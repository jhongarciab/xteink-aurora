#pragma once

namespace SilentTimeSync {

// Restores the X3 hardware clock and, when needed, briefly uses a visible saved
// network to refresh it from NTP. Returns true only after a successful NTP sync.
bool run(bool allowWifiAttempt);

}  // namespace SilentTimeSync
