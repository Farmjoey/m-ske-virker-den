#include "SettingsStorage.h"
#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>
#include <string.h>
#include "BoardConfig.h"
#include "Diagnostics.h"

static Preferences s_prefs;

void SettingsStorage::begin() {
  s_prefs.begin("nova", false);
  s_.demo = s_prefs.getBool("demo", true);
  s_.bleScan = s_prefs.getBool("ble", false);
  s_.invert = s_prefs.getBool("inv", NOVA_DEFAULT_INVERT != 0);
  s_.rotation = s_prefs.getUChar("rot", 1);
  if (s_.rotation != 1 && s_.rotation != 3) s_.rotation = 1;
  s_.backlightPct = s_prefs.getUChar("bl", 80);
  if (s_.backlightPct < 10) s_.backlightPct = 10;
  if (s_.backlightPct > 100) s_.backlightPct = 100;
  s_.hwConfirmed = s_prefs.getBool("hwok", false);
  {
    String a = s_prefs.getString("taddr", "");
    String n = s_prefs.getString("tname", "");
    strlcpy(s_.targetAddr, a.c_str(), sizeof(s_.targetAddr));
    strlcpy(s_.targetName, n.c_str(), sizeof(s_.targetName));
    s_.targetType = s_prefs.getUChar("ttype", 0);
  }

  // A different hardware profile was flashed: the old hardware test no longer applies.
  String stored = s_prefs.getString("prof", "");
  if (!stored.equals(NOVA_PROFILE_NAME)) {
    Diag::log("profile changed -> HW test required");
    s_.hwConfirmed = false;
    s_.invert = (NOVA_DEFAULT_INVERT != 0);
    s_prefs.putString("prof", NOVA_PROFILE_NAME);
    save();
  }
}

void SettingsStorage::save() {
  s_prefs.putBool("demo", s_.demo);
  s_prefs.putBool("ble", s_.bleScan);
  s_prefs.putBool("inv", s_.invert);
  s_prefs.putUChar("rot", s_.rotation);
  s_prefs.putUChar("bl", s_.backlightPct);
  s_prefs.putBool("hwok", s_.hwConfirmed);
  s_prefs.putString("taddr", s_.targetAddr);
  s_prefs.putString("tname", s_.targetName);
  s_prefs.putUChar("ttype", s_.targetType);
}

bool SettingsStorage::loadCal(uint8_t rotation, TouchCal& out) {
  char key[8];
  snprintf(key, sizeof(key), "cal%u", (unsigned)rotation);
  if (s_prefs.getBytesLength(key) != sizeof(TouchCal)) return false;
  TouchCal tmp;
  s_prefs.getBytes(key, &tmp, sizeof(TouchCal));
  if (tmp.magic != 0xC1 || !tmp.valid) return false;
  out = tmp;
  return true;
}

void SettingsStorage::saveCal(uint8_t rotation, const TouchCal& cal) {
  char key[8];
  snprintf(key, sizeof(key), "cal%u", (unsigned)rotation);
  s_prefs.putBytes(key, &cal, sizeof(TouchCal));
}
