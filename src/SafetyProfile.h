#pragma once
#include "SafetyLogic.h"
#include "ScooterProtocol.h"

enum class QuickResult : uint8_t { NotSupported, DemoOnly, Sent };

// Holds the LOCAL demo state of the 20 km/h quick profile.
// It never sends anything to the scooter (ScooterProtocol cannot write).
class SafetyProfile {
 public:
  SafetyProfile() : demoArmed_(false) {}

  LimitStatus status(bool demoMode) const {
    SafetyInputs in;
    in.protocolVerified = ScooterProtocol::kVerified;
    in.linkUp = false;       // no controller link exists in this build
    in.ackReceived = false;  // nothing can be acknowledged
    in.demoMode = demoMode;
    return computeLimitStatus(in);
  }

  QuickResult requestQuickLimit(bool demoMode) {
    const LimitStatus st = status(demoMode);
    if (st == LimitStatus::DemoSimulated) { demoArmed_ = true; return QuickResult::DemoOnly; }
    demoArmed_ = false;
    return QuickResult::NotSupported;  // real write path intentionally absent
  }

  void clearDemo() { demoArmed_ = false; }
  bool demoArmed() const { return demoArmed_; }

 private:
  bool demoArmed_;
};
