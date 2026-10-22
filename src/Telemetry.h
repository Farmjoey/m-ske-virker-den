#pragma once
// Scooter telemetry snapshot. Every field has its own validity flag.
// Nothing is ever shown as a value unless its *Valid flag is true.
#include <stdint.h>

struct Telemetry {
  bool speedValid;   float speedKmh;
  bool battPctValid; uint8_t battPct;
  bool voltValid;    float volts;
  bool powerValid;   int powerW;
  bool tripValid;    float tripKm;
  bool totalValid;   float totalKm;
  bool faultValid;   uint16_t faultCode;
  bool modeValid;    char mode[12];
  bool isDemo;       // true only for locally generated test data

  Telemetry()
      : speedValid(false), speedKmh(0), battPctValid(false), battPct(0),
        voltValid(false), volts(0), powerValid(false), powerW(0),
        tripValid(false), tripKm(0), totalValid(false), totalKm(0),
        faultValid(false), faultCode(0), modeValid(false), isDemo(false) {
    mode[0] = 0;
  }
};
