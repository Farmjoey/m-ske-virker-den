#pragma once
// Pure safety-profile rules. A limit is NEVER reported as confirmed unless the
// protocol is verified AND the link is up AND the controller acknowledged it.
#include <stdint.h>

enum class LimitStatus : uint8_t { Unknown, NotSupported, DemoSimulated, Confirmed };

struct SafetyInputs {
  bool protocolVerified;
  bool linkUp;
  bool ackReceived;
  bool demoMode;
};

static const int kQuickLimitKmh = 20;

inline LimitStatus computeLimitStatus(const SafetyInputs& in) {
  if (!in.protocolVerified)
    return in.demoMode ? LimitStatus::DemoSimulated : LimitStatus::NotSupported;
  if (in.linkUp && in.ackReceived) return LimitStatus::Confirmed;
  return LimitStatus::Unknown;
}

// The quick profile may only ever LOWER (or keep) the limit, and only to 20.
inline bool mayRequestLimit(int targetKmh, bool currentKnown, int currentKmh) {
  if (targetKmh != kQuickLimitKmh) return false;
  if (currentKnown && targetKmh > currentKmh) return false;  // would raise the limit
  return true;
}
