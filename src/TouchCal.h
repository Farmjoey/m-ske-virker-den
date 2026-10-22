#pragma once
// Pure 3-point touch calibration (TL, TR, BR). Auto-detects swapped axes and
// mirrored directions, so it works for any rotation / panel orientation.
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

struct RawPt { int16_t x; int16_t y; };

struct TouchCal {
  uint8_t magic;
  bool valid, swapXY;
  float ax, bx, ay, by;
  TouchCal() : magic(0xC1), valid(false), swapXY(false), ax(0), bx(0), ay(0), by(0) {}

  static int16_t rx(const RawPt& p, bool swap) { return swap ? p.y : p.x; }
  static int16_t ry(const RawPt& p, bool swap) { return swap ? p.x : p.y; }

  // tl/tr/br = raw readings while the user touched targets at
  // (inset,inset), (w-inset,inset), (w-inset,h-inset).
  static bool compute(const RawPt& tl, const RawPt& tr, const RawPt& br,
                      int w, int h, int inset, TouchCal& out) {
    TouchCal c;
    c.swapXY = abs(tr.y - tl.y) > abs(tr.x - tl.x);
    const float dxr = (float)(rx(tr, c.swapXY) - rx(tl, c.swapXY));
    const float dyr = (float)(ry(br, c.swapXY) - ry(tr, c.swapXY));
    if (fabsf(dxr) < 300.0f || fabsf(dyr) < 300.0f) return false;  // implausible
    c.ax = (float)(w - 2 * inset) / dxr;
    c.bx = (float)inset - c.ax * (float)rx(tl, c.swapXY);
    c.ay = (float)(h - 2 * inset) / dyr;
    c.by = (float)inset - c.ay * (float)ry(tr, c.swapXY);
    c.valid = true;
    out = c;
    return true;
  }

  void map(int16_t rawX, int16_t rawY, int w, int h, int16_t& sx, int16_t& sy) const {
    float fx = ax * (float)(swapXY ? rawY : rawX) + bx;
    float fy = ay * (float)(swapXY ? rawX : rawY) + by;
    if (fx < 0) fx = 0;
    if (fx > (float)(w - 1)) fx = (float)(w - 1);
    if (fy < 0) fy = 0;
    if (fy > (float)(h - 1)) fy = (float)(h - 1);
    sx = (int16_t)(fx + 0.5f);
    sy = (int16_t)(fy + 0.5f);
  }
};
