#pragma once
// Ride controls: mode (walk/eco/drive/sport), front light, zero start, hazard lights.
// In DEMO mode they change local simulated state only. Outside demo mode nothing is
// sent and no state is claimed, because the scooter protocol is not verified.
#include <stdint.h>
#include "ScooterProtocol.h"

enum class RideMode : uint8_t { Walk, Eco, Drive, Sport };
enum class ControlResult : uint8_t { NotSupported, DemoOnly, Sent };

inline const char* rideModeName(RideMode m) {
  switch (m) {
    case RideMode::Walk: return "WALK";
    case RideMode::Eco: return "ECO";
    case RideMode::Drive: return "DRIVE";
    default: return "SPORT";
  }
}

// Speed caps used ONLY by the demo simulation (illustrative, not scooter data).
inline int demoModeCapKmh(RideMode m) {
  switch (m) {
    case RideMode::Walk: return 6;
    case RideMode::Eco: return 15;
    case RideMode::Drive: return 20;
    default: return 25;
  }
}

class ScooterControl {
 public:
  ScooterControl() : mode_(RideMode::Eco), light_(false), zero_(false), hazard_(false) {}

  RideMode mode() const { return mode_; }
  bool light() const { return light_; }
  bool zeroStart() const { return zero_; }
  bool hazard() const { return hazard_; }

  ControlResult setMode(RideMode m, bool demo) {
    if (demo) { mode_ = m; return ControlResult::DemoOnly; }
    if (!canWrite()) return ControlResult::NotSupported;
    return ScooterProtocol::writeMode((uint8_t)m) ? ControlResult::Sent : ControlResult::NotSupported;
  }
  ControlResult setLight(bool on, bool demo) {
    if (demo) { light_ = on; return ControlResult::DemoOnly; }
    if (!canWrite()) return ControlResult::NotSupported;
    return ScooterProtocol::writeLight(on) ? ControlResult::Sent : ControlResult::NotSupported;
  }
  ControlResult setZeroStart(bool on, bool demo) {
    if (demo) { zero_ = on; return ControlResult::DemoOnly; }
    if (!canWrite()) return ControlResult::NotSupported;
    return ScooterProtocol::writeZeroStart(on) ? ControlResult::Sent : ControlResult::NotSupported;
  }
  ControlResult setHazard(bool on, bool demo) {
    if (demo) { hazard_ = on; return ControlResult::DemoOnly; }
    if (!canWrite()) return ControlResult::NotSupported;
    return ScooterProtocol::writeHazard(on) ? ControlResult::Sent : ControlResult::NotSupported;
  }

 private:
  static bool canWrite() { return ScooterProtocol::kVerified && ScooterProtocol::kWriteSupported; }
  RideMode mode_;
  bool light_, zero_, hazard_;
};
