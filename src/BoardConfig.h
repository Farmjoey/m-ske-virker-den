#pragma once
// Hardware profile. Display driver / colour order are chosen per PlatformIO env
// (see platformio.ini); inversion and rotation are runtime settings.
#include <stdint.h>

#ifndef NOVA_PROFILE_NAME
#define NOVA_PROFILE_NAME "unknown"
#endif
#ifndef NOVA_DEFAULT_INVERT
#define NOVA_DEFAULT_INVERT 0
#endif
#ifndef NOVA_FW_VERSION
#define NOVA_FW_VERSION "dev"
#endif

namespace board {
// Touch controller (XPT2046) is read by bit-banging, on its own pins.
static const int kTouchCLK = 25;
static const int kTouchMOSI = 32;
static const int kTouchMISO = 39;
static const int kTouchCS = 33;
static const int kTouchIRQ = 36;
static const int kBacklightPin = 21;
static const int kBootButton = 0;
static const int kScreenW = 320;  // landscape (rotation 1 or 3)
static const int kScreenH = 240;

// Name fragments that make an advertising device a *candidate* scooter.
// This is only a hint for the scan list - it is NOT a verified identification.
static const char* const kScooterNameHints[] = {"e-wheels", "ewheels", "e2s", "urban"};
static const int kScooterNameHintCount = 4;
}  // namespace board
