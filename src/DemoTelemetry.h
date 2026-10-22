#pragma once
// Locally generated TEST data. Always flagged isDemo=true so the UI labels it.
#include <math.h>
#include <stdio.h>
#include "Telemetry.h"

inline Telemetry demoSample(uint32_t nowMs, int capKmh) {
  Telemetry t;
  t.isDemo = true;
  const float s = (float)nowMs / 1000.0f;
  float spd = 17.5f + 12.5f * sinf(s * 0.6f);  // 5..30 km/h
  if (capKmh > 0 && spd > (float)capKmh) spd = (float)capKmh;
  t.speedValid = true;   t.speedKmh = spd;
  const int pct = 100 - (int)((nowMs / 30000UL) % 70UL);
  t.battPctValid = true; t.battPct = (uint8_t)pct;
  t.voltValid = true;    t.volts = 33.0f + (float)pct * 0.09f;
  t.powerValid = true;   t.powerW = (int)(spd * 14.0f);
  t.tripValid = true;    t.tripKm = (float)nowMs / 3600000.0f * 20.0f;
  t.totalValid = true;   t.totalKm = 1234.5f;
  t.modeValid = true;    snprintf(t.mode, sizeof(t.mode), "DEMO");
  t.faultValid = true;   t.faultCode = 0;
  return t;
}
