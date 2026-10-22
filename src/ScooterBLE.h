#pragma once
// BLE scanner + READ-ONLY XBOT link.
//  * Without a target: passive scan list (as before).
//  * With a target (chosen on the DIAG screen): connects, finds the transport profile
//    among the five documented ones, subscribes to notifications and sends the documented
//    read requests. It never writes any setting, light, mode or limit.
// All radio work runs in its own FreeRTOS task, so the UI never blocks.
#include <stdint.h>
#include "Telemetry.h"

enum class BleState : uint8_t { Off, Scanning, CandidateSeen, Connecting, Connected };

struct BleSeen {
  char name[20];
  char addr[18];
  int8_t rssi;
  bool candidate;     // name matches a hint - NOT a verified identification
  uint8_t addrType;
  BleSeen() : rssi(0), candidate(false), addrType(0) { name[0] = 0; addr[0] = 0; }
};

struct BleTarget {
  char addr[18];
  uint8_t type;
  char name[20];
  BleTarget() : type(0) { addr[0] = 0; name[0] = 0; }
  bool valid() const { return addr[0] != 0; }
};

struct BleLinkInfo {
  char profile[12];     // detected transport profile ("" = none)
  char name[20];        // name used for variant selection
  char version[10];     // controller version hex string, once read
  char triage[28];      // why no frames decoded (diagnosis only)
  char step[28];        // auto-detect progress ("testing ffe0 key 34")
  uint8_t variant;
  uint8_t key;
  bool refused;         // variant not decoded by this firmware -> nothing sent
  uint32_t notifies, frames;
  BleLinkInfo() : variant(0), key(0), refused(false), notifies(0), frames(0) {
    profile[0] = 0; name[0] = 0; version[0] = 0; triage[0] = 0; step[0] = 0;
  }
};

class ScooterBLE {
 public:
  ScooterBLE() {}
  void begin();                       // creates the BLE task (idle until enabled)
  void setEnabled(bool on);
  bool enabled() const;
  void setTarget(const BleTarget& t); // empty target = scan mode
  BleTarget target() const;
  void loop(uint32_t) {}              // kept for the main loop; work happens in the task
  BleState state() const;
  uint8_t count() const;              // devices seen in the last 30 s
  bool get(uint8_t idx, BleSeen& out) const;
  // True only with a live link and frames newer than 5 s. Otherwise `out` stays all-invalid.
  bool getTelemetry(Telemetry& out, uint32_t nowMs) const;
  BleLinkInfo linkInfo() const;
};
