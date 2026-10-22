#pragma once
#include <stdint.h>

struct TouchState {
  bool down;
  int16_t rawX, rawY;
  TouchState() : down(false), rawX(0), rawY(0) {}
};

// XPT2046 read by bit-banging (CYD wires touch on separate pins). Non-blocking.
class TouchInput {
 public:
  TouchInput() : releaseCnt_(0) {}
  void begin();
  TouchState poll();
 private:
  uint16_t xfer(uint8_t cmd);
  bool readOnce(int16_t& x, int16_t& y);
  TouchState last_;
  uint8_t releaseCnt_;
};

// BOOT button (GPIO0): short press = 1, long press (>=1 s) = 2, otherwise 0.
class BootButton {
 public:
  BootButton() : down_(false), longFired_(false), t0_(0) {}
  void begin();
  int poll(uint32_t now);
 private:
  bool down_, longFired_;
  uint32_t t0_;
};
