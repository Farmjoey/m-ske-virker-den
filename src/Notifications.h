#pragma once
// iOS message notifications. NOT IMPLEMENTED in v0.1.
// Planned (unverified) route: Apple Notification Center Service (ANCS) over a bonded
// BLE link. Nothing is stored; the feature stays disabled until a real link delivers data.
#include <stdint.h>

class Notifications {
 public:
  void begin() {}
  void loop(uint32_t) {}
  bool available() const { return false; }
};
