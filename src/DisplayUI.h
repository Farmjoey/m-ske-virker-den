#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "Telemetry.h"
#include "Gesture.h"
#include "TouchCal.h"
#include "TouchInput.h"
#include "SettingsStorage.h"
#include "SafetyProfile.h"
#include "ScooterBLE.h"
#include "PhoneMedia.h"
#include "Notifications.h"
#include "ScooterControl.h"

enum class Screen : uint8_t { Cal, Hw, Dash, Music, Msg, Settings, Safety, Diag, Ctrl };

struct UiContext {
  SettingsStorage* store;
  ScooterBLE* ble;
  PhoneMedia* phone;
  Notifications* notes;
  SafetyProfile* safety;
  ScooterControl* ctrl;
};

class DisplayUI {
 public:
  struct Btn { int16_t x, y, w, h; };
  void begin(const UiContext& ctx);
  void update(const Telemetry& t, const TouchState& ts, uint32_t now);
  void onBootButton(bool longPress);  // short = toggle inversion, long = flip 180 deg

 private:
  // lifecycle / helpers
  void go(Screen s);
  void applyDisplaySettings();
  void setBacklight(uint8_t pct);
  void splash();
  void flipRotation();
  void toggleInvert();
  void startCal(Screen returnTo);
  void txt(uint8_t slot, int16_t x, int16_t y, uint8_t font, const char* s,
           uint16_t fg, uint8_t datum, int16_t padW);
  void drawBtn(const Btn& b, const char* label, bool enabled, uint16_t accent);
  void drawHeader(const char* title);
  void drawFull();

  // screens
  void drawCal();      void tickCal(const TouchState& ts, uint32_t now);
  void drawHw();       void tickHw(bool touching, int16_t mx, int16_t my, const GestureEvent& ev);
  void drawDash();     void tickDash(const Telemetry& t, const GestureEvent& ev);
  void drawMusic();    void tickMusic(const GestureEvent& ev);
  void drawMsg();      void tickMsg(const GestureEvent& ev);
  void drawSettings(); void tickSettings(const GestureEvent& ev);
  void drawSafety();   void tickSafety(bool touching, int16_t mx, int16_t my,
                                       const GestureEvent& ev, uint32_t now);
  void drawSafetyBody(LimitStatus st, bool armed);
  void drawCtrl();     void tickCtrl(const GestureEvent& ev);
  void drawDiag();     void tickDiag(const GestureEvent& ev, uint32_t now);
  void hwButtons();
  void drawCalTarget();

  TFT_eSPI tft_;
  UiContext c_;
  Screen screen_ = Screen::Dash;
  bool full_ = true;
  GestureDetector gest_;
  HoldTracker hold_;
  TouchCal cal_;

  // colours (set in begin)
  uint16_t cBg_ = 0, cPanel_ = 0, cNeon_ = 0, cGlow_ = 0, cDim_ = 0, cWarn_ = 0, cOk_ = 0, cBad_ = 0, cText_ = 0;

  // text cache (redraw only on change)
  char cache_[16][44];
  uint16_t cacheCol_[16];

  // per-screen state
  uint8_t hwPage_ = 0;
  uint8_t calStep_ = 0;
  RawPt calPts_[3];
  bool calTouching_ = false, calWaitRelease_ = false;
  uint32_t calT0_ = 0;
  long calSumX_ = 0, calSumY_ = 0;
  int calN_ = 0;
  char calMsg_[40];
  Screen calReturn_ = Screen::Dash;
  float gauge_ = 0;
  int gaugeW_ = 0;
  uint8_t safetyKey_ = 255;
  int holdDrawn_ = 0;
  uint32_t lastDiagMs_ = 0;
};
