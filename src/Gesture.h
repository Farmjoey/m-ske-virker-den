#pragma once
// Pure touch-gesture logic (no hardware access) so it can be unit-tested on a PC.
#include <stdint.h>
#include <stdlib.h>

enum class GestureType : uint8_t { None, Tap, SwipeUp, SwipeDown, SwipeLeft, SwipeRight };

struct GestureEvent {
  GestureType type;
  int16_t x, y;  // start point of the gesture
  GestureEvent() : type(GestureType::None), x(0), y(0) {}
};

class GestureDetector {
 public:
  static const int kSwipeMinPx = 50;
  static const uint32_t kSwipeMaxMs = 700;
  static const int kTapSlopPx = 14;
  static const uint32_t kTapMaxMs = 800;

  GestureDetector() : down_(false), sx_(0), sy_(0), lx_(0), ly_(0), t0_(0) {}

  // Call every frame. Returns an event only on release.
  GestureEvent update(bool down, int16_t x, int16_t y, uint32_t now) {
    GestureEvent ev;
    if (down) {
      if (!down_) { down_ = true; sx_ = x; sy_ = y; t0_ = now; }
      lx_ = x; ly_ = y;
      return ev;
    }
    if (!down_) return ev;
    down_ = false;
    const int dx = lx_ - sx_, dy = ly_ - sy_;
    const int adx = abs(dx), ady = abs(dy);
    const uint32_t dt = now - t0_;
    ev.x = sx_; ev.y = sy_;
    if (dt <= kSwipeMaxMs && ady >= kSwipeMinPx && ady >= 2 * adx)
      ev.type = dy < 0 ? GestureType::SwipeUp : GestureType::SwipeDown;
    else if (dt <= kSwipeMaxMs && adx >= kSwipeMinPx && adx >= 2 * ady)
      ev.type = dx > 0 ? GestureType::SwipeRight : GestureType::SwipeLeft;
    else if (adx <= kTapSlopPx && ady <= kTapSlopPx && dt <= kTapMaxMs)
      ev.type = GestureType::Tap;
    return ev;
  }

 private:
  bool down_;
  int16_t sx_, sy_, lx_, ly_;
  uint32_t t0_;
};

// Press-and-hold tracker used to prevent accidental activation.
class HoldTracker {
 public:
  HoldTracker() : active_(false), fired_(false), t0_(0) {}
  // Returns true exactly once when the hold time has been reached.
  bool update(bool inside, uint32_t now, uint32_t holdMs) {
    if (!inside) { active_ = false; fired_ = false; return false; }
    if (!active_) { active_ = true; fired_ = false; t0_ = now; }
    if (!fired_ && now - t0_ >= holdMs) { fired_ = true; return true; }
    return false;
  }
  float progress(uint32_t now, uint32_t holdMs) const {
    if (!active_) return 0.0f;
    float p = (float)(now - t0_) / (float)holdMs;
    return p > 1.0f ? 1.0f : p;
  }
 private:
  bool active_, fired_;
  uint32_t t0_;
};
