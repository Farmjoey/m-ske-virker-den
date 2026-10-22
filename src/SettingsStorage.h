#pragma once
#include <stdint.h>
#include "TouchCal.h"

struct Settings {
  bool demo;            // demo/test telemetry (default ON until real protocol exists)
  bool bleScan;         // passive BLE scan list
  bool invert;          // panel colour inversion
  uint8_t rotation;     // 1 or 3 (landscape)
  uint8_t backlightPct; // 10..100
  bool hwConfirmed;     // user accepted the hardware test for this profile
  char targetAddr[18];  // chosen scooter (BLE address), empty = none
  uint8_t targetType;
  char targetName[20];
  Settings() : demo(true), bleScan(false), invert(false), rotation(1), backlightPct(80), hwConfirmed(false), targetType(0) { targetAddr[0] = 0; targetName[0] = 0; }
};

class SettingsStorage {
 public:
  void begin();
  Settings& get() { return s_; }
  void save();
  bool loadCal(uint8_t rotation, TouchCal& out);
  void saveCal(uint8_t rotation, const TouchCal& cal);
 private:
  Settings s_;
};
