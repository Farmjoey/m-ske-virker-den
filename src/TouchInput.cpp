#include "TouchInput.h"
#include <Arduino.h>
#include "BoardConfig.h"

void TouchInput::begin() {
  pinMode(board::kTouchCS, OUTPUT);
  digitalWrite(board::kTouchCS, HIGH);
  pinMode(board::kTouchCLK, OUTPUT);
  digitalWrite(board::kTouchCLK, LOW);
  pinMode(board::kTouchMOSI, OUTPUT);
  pinMode(board::kTouchMISO, INPUT);
  pinMode(board::kTouchIRQ, INPUT);  // GPIO36 is input-only; board provides the pull-up
}

// Mode-0 SPI by hand: 8 command bits, then 16 clocks of which 12 are the result.
uint16_t TouchInput::xfer(uint8_t cmd) {
  digitalWrite(board::kTouchCS, LOW);
  for (int i = 7; i >= 0; --i) {
    digitalWrite(board::kTouchMOSI, (cmd >> i) & 1);
    digitalWrite(board::kTouchCLK, HIGH);
    digitalWrite(board::kTouchCLK, LOW);
  }
  uint16_t v = 0;
  for (int i = 0; i < 16; ++i) {
    digitalWrite(board::kTouchCLK, HIGH);
    v = (uint16_t)((v << 1) | (digitalRead(board::kTouchMISO) ? 1 : 0));
    digitalWrite(board::kTouchCLK, LOW);
  }
  digitalWrite(board::kTouchCS, HIGH);
  return (uint16_t)((v >> 3) & 0x0FFF);
}

static int16_t trimmedMean(int16_t* a, int n) {  // drop min & max, average the rest
  int mn = 0, mx = 0;
  for (int i = 1; i < n; ++i) { if (a[i] < a[mn]) mn = i; if (a[i] > a[mx]) mx = i; }
  long sum = 0; int cnt = 0;
  for (int i = 0; i < n; ++i) { if (i == mn || i == mx) continue; sum += a[i]; ++cnt; }
  return cnt ? (int16_t)(sum / cnt) : a[0];
}

bool TouchInput::readOnce(int16_t& x, int16_t& y) {
  int16_t xs[5], ys[5];
  for (int i = 0; i < 5; ++i) {
    xs[i] = (int16_t)xfer(0xD0);  // X, 12-bit, differential, power-down (keeps PENIRQ alive)
    ys[i] = (int16_t)xfer(0x90);  // Y
  }
  x = trimmedMean(xs, 5);
  y = trimmedMean(ys, 5);
  return x > 80 && x < 4010 && y > 80 && y < 4010;  // reject rails / floating readings
}

TouchState TouchInput::poll() {
  const bool irq = (digitalRead(board::kTouchIRQ) == LOW);
  if (irq) {
    int16_t x, y;
    if (readOnce(x, y)) {
      last_.down = true; last_.rawX = x; last_.rawY = y; releaseCnt_ = 0;
      return last_;
    }
  }
  if (last_.down) {                       // debounce release: 3 consecutive misses
    if (++releaseCnt_ >= 3) { last_.down = false; releaseCnt_ = 0; }
  }
  return last_;
}

void BootButton::begin() { pinMode(board::kBootButton, INPUT_PULLUP); }

int BootButton::poll(uint32_t now) {
  const bool p = (digitalRead(board::kBootButton) == LOW);
  int r = 0;
  if (p && !down_) { down_ = true; longFired_ = false; t0_ = now; }
  else if (p && down_ && !longFired_ && now - t0_ >= 1000) { longFired_ = true; r = 2; }
  else if (!p && down_) { down_ = false; if (!longFired_ && now - t0_ >= 30) r = 1; }
  return r;
}
