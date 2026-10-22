#include "DisplayUI.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <esp_system.h>
#include "BoardConfig.h"
#include "Format.h"
#include "ScooterProtocol.h"
#include "Diagnostics.h"

namespace {
const int16_t kW = 320;
const int16_t kH = 240;
const uint32_t kHoldMs = 1500;

DisplayUI::Btn mk(int16_t x, int16_t y, int16_t w, int16_t h) {
  DisplayUI::Btn b; b.x = x; b.y = y; b.w = w; b.h = h; return b;
}
DisplayUI::Btn bottomBtn(int i) { return mk((int16_t)(8 + i * 104), 206, 96, 30); }
DisplayUI::Btn dashBtn(int i) { return mk((int16_t)(6 + i * 80), 206, 72, 30); }
DisplayUI::Btn gridBtn(int i) { return mk((int16_t)(8 + (i % 3) * 104), (int16_t)(38 + (i / 3) * 56), 96, 48); }
DisplayUI::Btn holdBtn() { return mk(40, 150, 240, 40); }
bool inBtn(const DisplayUI::Btn& b, int16_t x, int16_t y) {
  return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}
}  // namespace

// ------------------------------------------------------------------ lifecycle

void DisplayUI::begin(const UiContext& ctx) {
  c_ = ctx;
  memset(cache_, 0, sizeof(cache_));
  memset(cacheCol_, 0, sizeof(cacheCol_));
  calMsg_[0] = 0;
  tft_.init();
  cBg_ = tft_.color565(4, 8, 20);
  cPanel_ = tft_.color565(10, 22, 48);
  cNeon_ = tft_.color565(0, 170, 255);
  cGlow_ = tft_.color565(0, 60, 100);
  cDim_ = tft_.color565(90, 110, 140);
  cWarn_ = tft_.color565(255, 176, 0);
  cOk_ = tft_.color565(0, 220, 140);
  cBad_ = tft_.color565(255, 70, 90);
  cText_ = tft_.color565(225, 240, 255);
  applyDisplaySettings();
  splash();

  const Settings& s = c_.store->get();
  hwPage_ = 0;
  if (!cal_.valid) startCal(s.hwConfirmed ? Screen::Dash : Screen::Hw);
  else if (!s.hwConfirmed) go(Screen::Hw);
  else go(Screen::Dash);
}

void DisplayUI::go(Screen s) {
  screen_ = s;
  full_ = true;
  gest_ = GestureDetector();
  hold_ = HoldTracker();
}

void DisplayUI::setBacklight(uint8_t pct) {
  static bool attached = false;
  if (!attached) {
    ledcSetup(0, 5000, 8);
    ledcAttachPin(board::kBacklightPin, 0);
    attached = true;
  }
  if (pct < 10) pct = 10;
  if (pct > 100) pct = 100;
  ledcWrite(0, (uint32_t)pct * 255u / 100u);
}

void DisplayUI::applyDisplaySettings() {
  const Settings& s = c_.store->get();
  tft_.setRotation(s.rotation);
  tft_.invertDisplay(s.invert);
  setBacklight(s.backlightPct);
  TouchCal tc;
  if (c_.store->loadCal(s.rotation, tc)) cal_ = tc; else cal_ = TouchCal();
}

void DisplayUI::splash() {
  tft_.fillScreen(cBg_);
  tft_.setTextPadding(0);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(cNeon_, cBg_);
  tft_.setTextSize(2);
  tft_.drawString("NOVA", 160, 80, 4);
  tft_.setTextSize(1);
  tft_.setTextColor(cText_, cBg_);
  tft_.drawString("RIDE OS", 160, 130, 4);
  tft_.setTextColor(cDim_, cBg_);
  tft_.drawString(NOVA_FW_VERSION " / " NOVA_PROFILE_NAME, 160, 205, 2);
  for (int x = 40; x <= 280; x += 8) {
    tft_.drawFastHLine(40, 160, x - 40, cNeon_);
    delay(6);
  }
  delay(250);
}

void DisplayUI::toggleInvert() {
  Settings& s = c_.store->get();
  s.invert = !s.invert;
  tft_.invertDisplay(s.invert);
  c_.store->save();
  full_ = true;
}

void DisplayUI::flipRotation() {
  Settings& s = c_.store->get();
  s.rotation = (s.rotation == 1) ? 3 : 1;
  c_.store->save();
  applyDisplaySettings();
  const Screen ret = (screen_ == Screen::Cal) ? calReturn_ : screen_;
  if (cal_.valid) go(ret); else startCal(ret);
}

void DisplayUI::onBootButton(bool longPress) {
  if (longPress) flipRotation(); else toggleInvert();
}

void DisplayUI::startCal(Screen returnTo) {
  calReturn_ = returnTo;
  screen_ = Screen::Cal;
  full_ = true;
  calStep_ = 0;
  calTouching_ = false;
  calWaitRelease_ = true;  // require a release before the first target counts
  gest_ = GestureDetector();
}

// ------------------------------------------------------------------ drawing helpers

void DisplayUI::txt(uint8_t slot, int16_t x, int16_t y, uint8_t font, const char* s,
                    uint16_t fg, uint8_t datum, int16_t padW) {
  if (slot >= 16) return;
  if (strcmp(cache_[slot], s) == 0 && cacheCol_[slot] == fg) return;
  tft_.setTextColor(fg, cBg_);
  tft_.setTextDatum(datum);
  tft_.setTextPadding(padW);
  tft_.drawString(s, x, y, font);
  tft_.setTextPadding(0);
  strlcpy(cache_[slot], s, sizeof(cache_[slot]));
  cacheCol_[slot] = fg;
}

void DisplayUI::drawBtn(const Btn& b, const char* label, bool enabled, uint16_t accent) {
  tft_.fillRoundRect(b.x, b.y, b.w, b.h, 6, cPanel_);
  tft_.drawRoundRect(b.x, b.y, b.w, b.h, 6, enabled ? accent : cDim_);
  tft_.setTextColor(enabled ? cText_ : cDim_, cPanel_);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextPadding(0);
  tft_.drawString(label, b.x + b.w / 2, b.y + b.h / 2, 2);
}

void DisplayUI::drawHeader(const char* title) {
  tft_.fillScreen(cBg_);
  tft_.setTextDatum(TC_DATUM);
  tft_.setTextPadding(0);
  tft_.setTextColor(cNeon_, cBg_);
  tft_.drawString(title, 160, 3, 4);
  tft_.drawFastHLine(0, 32, kW, cNeon_);
  tft_.drawFastHLine(0, 33, kW, cGlow_);
}

void DisplayUI::drawFull() {
  memset(cache_, 0, sizeof(cache_));
  memset(cacheCol_, 0, sizeof(cacheCol_));
  switch (screen_) {
    case Screen::Cal: drawCal(); break;
    case Screen::Hw: drawHw(); break;
    case Screen::Dash: drawDash(); break;
    case Screen::Music: drawMusic(); break;
    case Screen::Msg: drawMsg(); break;
    case Screen::Settings: drawSettings(); break;
    case Screen::Safety: drawSafety(); break;
    case Screen::Diag: drawDiag(); break;
    case Screen::Ctrl: drawCtrl(); break;
  }
}

// ------------------------------------------------------------------ main update

void DisplayUI::update(const Telemetry& t, const TouchState& ts, uint32_t now) {
  if (screen_ == Screen::Cal) { tickCal(ts, now); return; }

  int16_t mx = 0, my = 0;
  const bool touching = ts.down && cal_.valid;
  if (touching) cal_.map(ts.rawX, ts.rawY, kW, kH, mx, my);

  if (full_) { drawFull(); full_ = false; }
  const GestureEvent ev = gest_.update(touching, mx, my, now);

  switch (screen_) {
    case Screen::Hw: tickHw(touching, mx, my, ev); break;
    case Screen::Dash: tickDash(t, ev); break;
    case Screen::Music: tickMusic(ev); break;
    case Screen::Msg: tickMsg(ev); break;
    case Screen::Settings: tickSettings(ev); break;
    case Screen::Safety: tickSafety(touching, mx, my, ev, now); break;
    case Screen::Diag: tickDiag(ev, now); break;
    case Screen::Ctrl: tickCtrl(ev); break;
    default: break;
  }
}

// ------------------------------------------------------------------ calibration

void DisplayUI::drawCalTarget() {
  int16_t tx = 20, ty = 20;
  if (calStep_ == 1) { tx = kW - 20; ty = 20; }
  if (calStep_ == 2) { tx = kW - 20; ty = kH - 20; }
  tft_.drawFastHLine(tx - 14, ty, 29, cNeon_);
  tft_.drawFastVLine(tx, ty - 14, 29, cNeon_);
  tft_.drawCircle(tx, ty, 7, cNeon_);
  tft_.fillCircle(tx, ty, 2, cText_);
}

void DisplayUI::drawCal() {
  tft_.fillScreen(cBg_);
  tft_.setTextPadding(0);
  tft_.setTextDatum(MC_DATUM);
  tft_.setTextColor(cText_, cBg_);
  char b[32];
  snprintf(b, sizeof(b), "TOUCH CALIBRATION %u/3", (unsigned)(calStep_ + 1));
  tft_.drawString(b, 160, 100, 2);
  tft_.setTextColor(cDim_, cBg_);
  tft_.drawString("Tap the centre of the crosshair", 160, 122, 2);
  tft_.drawString("BOOT: invert (short) / flip (long)", 160, 142, 2);
  if (calMsg_[0]) {
    tft_.setTextColor(cBad_, cBg_);
    tft_.drawString(calMsg_, 160, 170, 2);
  }
  drawCalTarget();
}

void DisplayUI::tickCal(const TouchState& ts, uint32_t now) {
  if (full_) { drawCal(); full_ = false; }
  if (calWaitRelease_) { if (!ts.down) calWaitRelease_ = false; return; }
  if (!ts.down) { calTouching_ = false; return; }
  if (!calTouching_) { calTouching_ = true; calT0_ = now; calSumX_ = 0; calSumY_ = 0; calN_ = 0; }
  calSumX_ += ts.rawX; calSumY_ += ts.rawY; ++calN_;
  if (now - calT0_ < 150 || calN_ <= 0) return;

  calPts_[calStep_].x = (int16_t)(calSumX_ / calN_);
  calPts_[calStep_].y = (int16_t)(calSumY_ / calN_);
  calTouching_ = false;
  calWaitRelease_ = true;
  if (++calStep_ < 3) { full_ = true; return; }

  TouchCal tc;
  if (TouchCal::compute(calPts_[0], calPts_[1], calPts_[2], kW, kH, 20, tc)) {
    cal_ = tc;
    c_.store->saveCal(c_.store->get().rotation, tc);
    calMsg_[0] = 0;
    Diag::log("cal ok swap=%d", tc.swapXY ? 1 : 0);
    go(calReturn_);
  } else {
    strlcpy(calMsg_, "Calibration failed - try again", sizeof(calMsg_));
    Diag::log("cal failed");
    calStep_ = 0;
    full_ = true;
  }
}

// ------------------------------------------------------------------ hardware test

void DisplayUI::hwButtons() {
  const char* a = ""; const char* b = ""; const char* c = "";
  switch (hwPage_) {
    case 0: case 1: a = "INVERT"; b = "FLIP"; c = "NEXT"; break;
    case 2: a = "CALIBRATE"; b = "CLEAR"; c = "NEXT"; break;
    default: a = "BACK"; b = ""; c = "ACCEPT"; break;
  }
  if (a[0]) drawBtn(bottomBtn(0), a, true, cNeon_);
  if (b[0]) drawBtn(bottomBtn(1), b, true, cNeon_);
  if (c[0]) drawBtn(bottomBtn(2), c, true, hwPage_ == 3 ? cOk_ : cNeon_);
}

void DisplayUI::drawHw() {
  tft_.fillScreen(cBg_);
  tft_.setTextPadding(0);
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(cText_, cBg_);
  char h[48];
  switch (hwPage_) {
    case 0: {
      snprintf(h, sizeof(h), "HW 1/4 COLORS  %s", NOVA_PROFILE_NAME);
      tft_.drawString(h, 4, 4, 2);
      const uint16_t cols[8] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK, TFT_CYAN, TFT_MAGENTA, TFT_YELLOW};
      const char* names[8] = {"RED", "GRN", "BLU", "WHT", "BLK", "CYN", "MAG", "YEL"};
      for (int i = 0; i < 8; ++i) tft_.fillRect(i * 40, 24, 40, 126, cols[i]);
      tft_.drawRect(4 * 40, 24, 40, 126, cDim_);
      tft_.setTextDatum(TC_DATUM);
      for (int i = 0; i < 8; ++i) tft_.drawString(names[i], i * 40 + 20, 154, 2);
      tft_.setTextDatum(TL_DATUM);
      tft_.setTextColor(cDim_, cBg_);
      tft_.drawString("Negative/odd colours? tap INVERT", 4, 174, 2);
      tft_.drawString("RED looks blue? flash *_swaprb build", 4, 190, 2);
      break;
    }
    case 1: {
      tft_.drawString("HW 2/4 ORIENTATION", 4, 4, 2);
      tft_.drawRect(0, 0, kW, kH, cNeon_);
      tft_.drawRect(1, 1, kW - 2, kH - 2, cGlow_);
      tft_.drawLine(0, 0, kW - 1, kH - 1, cGlow_);
      tft_.drawLine(kW - 1, 0, 0, kH - 1, cGlow_);
      tft_.setTextColor(cText_, cBg_);
      tft_.drawString("TL", 8, 24, 2);
      tft_.setTextDatum(TR_DATUM); tft_.drawString("TR", kW - 8, 24, 2);
      tft_.setTextDatum(TL_DATUM); tft_.drawString("BL", 8, 180, 2);
      tft_.setTextDatum(TR_DATUM); tft_.drawString("BR", kW - 8, 180, 2);
      tft_.setTextDatum(MC_DATUM);
      tft_.setTextColor(cNeon_, cBg_);
      tft_.drawString("NOVA 123", 160, 100, 4);
      char d[32];
      snprintf(d, sizeof(d), "%dx%d rot %u", (int)tft_.width(), (int)tft_.height(),
               (unsigned)c_.store->get().rotation);
      tft_.setTextColor(cText_, cBg_);
      tft_.drawString(d, 160, 132, 2);
      tft_.setTextColor(cDim_, cBg_);
      tft_.drawString("Text must read normally", 160, 152, 2);
      break;
    }
    case 2:
      tft_.drawString("HW 3/4 TOUCH", 4, 4, 2);
      tft_.setTextColor(cDim_, cBg_);
      tft_.drawString("Touch the screen: dot must follow finger", 4, 24, 2);
      tft_.drawString("Wrong? CALIBRATE again (BOOT = flip)", 4, 44, 2);
      break;
    default:
      tft_.drawString("HW 4/4 ACCEPT", 4, 4, 2);
      tft_.setTextColor(cDim_, cBg_);
      tft_.drawString("Colours, orientation, touch all OK?", 4, 34, 2);
      tft_.drawString("ACCEPT stores this profile on the device.", 4, 54, 2);
      tft_.setTextColor(cWarn_, cBg_);
      tft_.drawString("Scooter link: NOT SUPPORTED", 4, 94, 2);
      tft_.drawString("Dashboard starts in DEMO mode.", 4, 114, 2);
      break;
  }
  hwButtons();
}

void DisplayUI::tickHw(bool touching, int16_t mx, int16_t my, const GestureEvent& ev) {
  Settings& s = c_.store->get();
  if (hwPage_ == 2 && touching && my < 200) {
    tft_.fillCircle(mx, my, 3, cNeon_);
    char b[44];
    snprintf(b, sizeof(b), "map %3d,%3d  rot %u", (int)mx, (int)my, (unsigned)s.rotation);
    txt(0, 8, 176, 2, b, cText_, TL_DATUM, 300);
  }
  if (ev.type != GestureType::Tap) return;
  int idx = -1;
  for (int i = 0; i < 3; ++i) if (inBtn(bottomBtn(i), ev.x, ev.y)) idx = i;
  if (idx < 0) return;
  switch (hwPage_) {
    case 0: case 1:
      if (idx == 0) toggleInvert();
      else if (idx == 1) flipRotation();
      else { ++hwPage_; full_ = true; }
      break;
    case 2:
      if (idx == 0) startCal(Screen::Hw);
      else if (idx == 1) full_ = true;
      else { hwPage_ = 3; full_ = true; }
      break;
    default:
      if (idx == 0) { hwPage_ = 0; full_ = true; }
      else if (idx == 2) { s.hwConfirmed = true; c_.store->save(); go(Screen::Dash); }
      break;
  }
}

// ------------------------------------------------------------------ dashboard

void DisplayUI::drawDash() {
  tft_.fillScreen(cBg_);
  tft_.drawFastHLine(0, 28, kW, cNeon_);
  tft_.drawFastHLine(0, 29, kW, cGlow_);
  tft_.setTextPadding(0);
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(cDim_, cBg_);
  tft_.drawString("km/h", 240, 88, 4);
  tft_.drawRoundRect(19, 116, 282, 12, 4, cDim_);
  tft_.fillRect(21, 118, 278, 8, cPanel_);
  gaugeW_ = 0;
  tft_.setTextDatum(TC_DATUM);
  tft_.drawString("BATTERY", 60, 138, 2);
  tft_.drawString("VOLTAGE", 160, 138, 2);
  tft_.drawString("POWER", 260, 138, 2);
  drawBtn(dashBtn(0), "MUSIC", true, cNeon_);
  drawBtn(dashBtn(1), "MSG", true, cNeon_);
  drawBtn(dashBtn(2), "CTRL", true, cNeon_);
  drawBtn(dashBtn(3), "SET", true, cNeon_);
}

void DisplayUI::tickDash(const Telemetry& t, const GestureEvent& ev) {
  // header: BLE state, data source badge, phone, clock (no RTC -> not available)
  const BleState bs = c_.ble->state();
  const char* bt = "BLE OFF";
  uint16_t bc = cDim_;
  switch (bs) {
    case BleState::Scanning: bt = "BLE SCAN"; bc = cNeon_; break;
    case BleState::CandidateSeen: bt = "BLE:SCOOTER?"; bc = cWarn_; break;
    case BleState::Connecting: bt = "BLE CONN..."; bc = cWarn_; break;
    case BleState::Connected: bt = "BLE LINK"; bc = cOk_; break;
    default: break;
  }
  txt(0, 6, 7, 2, bt, bc, TL_DATUM, 112);
  if (t.isDemo) txt(1, 160, 7, 2, "DEMO", cWarn_, TC_DATUM, 56);
  else if (t.speedValid || t.battPctValid || t.voltValid) txt(1, 160, 7, 2, "LIVE", cOk_, TC_DATUM, 56);
  else txt(1, 160, 7, 2, "NO DATA", cBad_, TC_DATUM, 64);
  txt(2, 196, 7, 2, "PHONE --", cDim_, TL_DATUM, 60);
  txt(3, 314, 7, 2, "--:--", cDim_, TR_DATUM, 50);

  // speed (large). Invalid -> two dashes drawn as shapes.
  char b[24], n[12];
  if (t.speedValid) {
    fmtFixed(n, sizeof(n), true, t.speedKmh, 0);
    txt(4, 150, 34, 8, n, cText_, TC_DATUM, 150);
  } else if (strcmp(cache_[4], "--") != 0) {
    tft_.fillRect(75, 34, 150, 78, cBg_);
    tft_.fillRect(112, 68, 30, 8, cDim_);
    tft_.fillRect(158, 68, 30, 8, cDim_);
    strlcpy(cache_[4], "--", sizeof(cache_[4]));
  }

  // animated gauge (eased), 0..35 km/h
  const float target = t.speedValid ? fminf(t.speedKmh / 35.0f, 1.0f) : 0.0f;
  gauge_ += (target - gauge_) * 0.3f;
  const int w = (int)(gauge_ * 278.0f);
  if (w != gaugeW_) {
    if (w > gaugeW_) tft_.fillRect(21 + gaugeW_, 118, w - gaugeW_, 8, cNeon_);
    else tft_.fillRect(21 + w, 118, gaugeW_ - w, 8, cPanel_);
    gaugeW_ = w;
  }

  // battery / voltage / power
  fmtFixed(n, sizeof(n), t.battPctValid, (float)t.battPct, 0);
  snprintf(b, sizeof(b), t.battPctValid ? "%s%%" : "%s", n);
  uint16_t col = cDim_;
  if (t.battPctValid) col = t.battPct > 50 ? cOk_ : (t.battPct > 20 ? cWarn_ : cBad_);
  txt(5, 60, 156, 4, b, col, TC_DATUM, 100);
  fmtFixed(n, sizeof(n), t.voltValid, t.volts, 1);
  snprintf(b, sizeof(b), t.voltValid ? "%s V" : "%s", n);
  txt(6, 160, 156, 4, b, t.voltValid ? cText_ : cDim_, TC_DATUM, 100);
  fmtFixed(n, sizeof(n), t.powerValid, (float)t.powerW, 0);
  snprintf(b, sizeof(b), t.powerValid ? "%s W" : "%s", n);
  txt(7, 260, 156, 4, b, t.powerValid ? cText_ : cDim_, TC_DATUM, 100);

  // trip / total / mode or fault
  fmtFixed(n, sizeof(n), t.tripValid, t.tripKm, 1);
  snprintf(b, sizeof(b), "TRIP %s km", n);
  txt(8, 8, 186, 2, b, cDim_, TL_DATUM, 100);
  fmtFixed(n, sizeof(n), t.totalValid, t.totalKm, 0);
  snprintf(b, sizeof(b), "TOTAL %s km", n);
  txt(9, 112, 186, 2, b, cDim_, TL_DATUM, 100);
  if (t.faultValid && t.faultCode != 0) {
    snprintf(b, sizeof(b), "FAULT %u", (unsigned)t.faultCode);
    txt(10, 216, 186, 2, b, cBad_, TL_DATUM, 96);
  } else {
    snprintf(b, sizeof(b), "MODE %s", t.modeValid ? t.mode : "--");
    txt(10, 216, 186, 2, b, cDim_, TL_DATUM, 96);
  }

  // input
  if (ev.type == GestureType::SwipeUp) { go(Screen::Safety); return; }
  if (ev.type == GestureType::Tap) {
    if (inBtn(dashBtn(0), ev.x, ev.y)) go(Screen::Music);
    else if (inBtn(dashBtn(1), ev.x, ev.y)) go(Screen::Msg);
    else if (inBtn(dashBtn(2), ev.x, ev.y)) go(Screen::Ctrl);
    else if (inBtn(dashBtn(3), ev.x, ev.y)) go(Screen::Settings);
    else if (ev.x >= 216 && ev.y >= 180 && ev.y < 204) go(Screen::Ctrl);  // tap the mode text
  }
}

// ------------------------------------------------------------------ music / messages

void DisplayUI::drawMusic() {
  drawHeader("MUSIC");
  const MediaInfo& mi = c_.phone->info();
  tft_.setTextPadding(0);
  tft_.setTextDatum(TC_DATUM);
  tft_.setTextColor(cDim_, cBg_);
  const char* st = (mi.link == PhoneLink::Connected) ? "PHONE: CONNECTED"
                 : (mi.link == PhoneLink::Disconnected) ? "PHONE: NOT CONNECTED"
                 : "PHONE: NOT IMPLEMENTED";
  tft_.drawString(st, 160, 40, 2);
  tft_.setTextColor(cText_, cBg_);
  tft_.drawString(mi.trackValid ? mi.title : "--", 160, 66, 4);
  tft_.setTextColor(cDim_, cBg_);
  tft_.drawString(mi.trackValid ? mi.artist : "--", 160, 100, 2);
  const bool en = c_.phone->available();
  drawBtn(mk(8, 128, 96, 44), "PREV", en, cNeon_);
  drawBtn(mk(112, 128, 96, 44), "PLAY/PAUSE", en, cNeon_);
  drawBtn(mk(216, 128, 96, 44), "NEXT", en, cNeon_);
  tft_.setTextColor(cDim_, cBg_);
  tft_.drawString("Needs iPhone BLE media link (AMS).", 160, 180, 2);
  drawBtn(bottomBtn(0), "BACK", true, cNeon_);
}

void DisplayUI::tickMusic(const GestureEvent& ev) {
  if (ev.type != GestureType::Tap) return;
  if (inBtn(bottomBtn(0), ev.x, ev.y)) { go(Screen::Dash); return; }
  if (!c_.phone->available()) return;  // controls stay inert until a real link exists
  if (inBtn(mk(8, 128, 96, 44), ev.x, ev.y)) c_.phone->prev();
  else if (inBtn(mk(112, 128, 96, 44), ev.x, ev.y)) c_.phone->playPause();
  else if (inBtn(mk(216, 128, 96, 44), ev.x, ev.y)) c_.phone->next();
}

void DisplayUI::drawMsg() {
  drawHeader("MESSAGES");
  tft_.setTextPadding(0);
  tft_.setTextDatum(TC_DATUM);
  tft_.setTextColor(cWarn_, cBg_);
  tft_.drawString("NOT AVAILABLE", 160, 70, 4);
  tft_.setTextColor(cDim_, cBg_);
  tft_.drawString("Needs iOS notification link (ANCS).", 160, 114, 2);
  tft_.drawString("Not implemented. Nothing is stored.", 160, 134, 2);
  drawBtn(bottomBtn(0), "BACK", true, cNeon_);
}

void DisplayUI::tickMsg(const GestureEvent& ev) {
  if (ev.type == GestureType::Tap && inBtn(bottomBtn(0), ev.x, ev.y)) go(Screen::Dash);
}

// ------------------------------------------------------------------ settings

void DisplayUI::drawSettings() {
  drawHeader("SETTINGS");
  const Settings& s = c_.store->get();
  char l[16];
  snprintf(l, sizeof(l), "DEMO %s", s.demo ? "ON" : "OFF");        drawBtn(gridBtn(0), l, true, cNeon_);
  snprintf(l, sizeof(l), "BLE %s", s.bleScan ? "ON" : "OFF");      drawBtn(gridBtn(1), l, true, cNeon_);
  snprintf(l, sizeof(l), "INVERT %s", s.invert ? "ON" : "OFF");    drawBtn(gridBtn(2), l, true, cNeon_);
  drawBtn(gridBtn(3), "FLIP 180", true, cNeon_);
  snprintf(l, sizeof(l), "BRIGHT %u%%", (unsigned)s.backlightPct); drawBtn(gridBtn(4), l, true, cNeon_);
  drawBtn(gridBtn(5), "HW TEST", true, cNeon_);
  drawBtn(gridBtn(6), "CALIBRATE", true, cNeon_);
  drawBtn(gridBtn(7), "DIAG", true, cNeon_);
  drawBtn(gridBtn(8), "BACK", true, cOk_);
}

void DisplayUI::tickSettings(const GestureEvent& ev) {
  if (ev.type != GestureType::Tap) return;
  int idx = -1;
  for (int i = 0; i < 9; ++i) if (inBtn(gridBtn(i), ev.x, ev.y)) idx = i;
  if (idx < 0) return;
  Settings& s = c_.store->get();
  switch (idx) {
    case 0: s.demo = !s.demo; c_.store->save(); full_ = true; break;
    case 1: s.bleScan = !s.bleScan; c_.ble->setEnabled(s.bleScan); c_.store->save(); full_ = true; break;
    case 2: toggleInvert(); break;
    case 3: flipRotation(); break;
    case 4: {
      const uint8_t steps[4] = {25, 50, 75, 100};
      uint8_t next = steps[0];
      for (int i = 0; i < 4; ++i) if (steps[i] > s.backlightPct) { next = steps[i]; break; }
      s.backlightPct = next; setBacklight(next); c_.store->save(); full_ = true;
      break;
    }
    case 5: hwPage_ = 0; go(Screen::Hw); break;
    case 6: startCal(Screen::Settings); break;
    case 7: go(Screen::Diag); break;
    default: go(Screen::Dash); break;
  }
}

// ------------------------------------------------------------------ 20 km/h safety screen

void DisplayUI::drawSafety() {
  drawHeader("20 KM/H LIMIT");
  drawBtn(bottomBtn(0), "BACK", true, cNeon_);
  safetyKey_ = 255;  // body is (re)drawn by tickSafety
}

void DisplayUI::drawSafetyBody(LimitStatus st, bool armed) {
  tft_.fillRect(0, 36, kW, 166, cBg_);
  const char* stat = "";
  const char* l1 = ""; const char* l2 = ""; const char* l3 = "";
  uint16_t col = cDim_;
  switch (st) {
    case LimitStatus::Confirmed:
      col = cOk_; stat = "CONFIRMED 20 KM/H"; l1 = "Controller acknowledged."; break;
    case LimitStatus::DemoSimulated:
      if (armed) { col = cWarn_; stat = "DEMO: 20 SIMULATED"; l1 = "DEMO ONLY - nothing was sent."; l2 = "Scooter limit: NOT CHANGED"; l3 = "Tap below to clear."; }
      else { col = cDim_; stat = "DEMO: NOT ARMED"; l1 = "DEMO ONLY - nothing is sent."; l2 = "Scooter limit: UNKNOWN"; l3 = "Hold button to simulate 20."; }
      break;
    case LimitStatus::NotSupported:
      col = cBad_; stat = "NOT CONFIRMED"; l1 = "Controller link not supported."; l2 = "No command is ever sent."; l3 = "Scooter limit: UNKNOWN"; break;
    default:
      col = cWarn_; stat = "UNKNOWN"; l1 = "No link / no confirmation."; l2 = "Limit state: UNKNOWN"; break;
  }
  tft_.setTextPadding(0);
  tft_.setTextDatum(TC_DATUM);
  tft_.setTextColor(col, cBg_);
  tft_.drawString(stat, 160, 42, 4);
  tft_.setTextColor(cText_, cBg_);
  tft_.drawString(l1, 160, 80, 2);
  tft_.setTextColor(cDim_, cBg_);
  tft_.drawString(l2, 160, 100, 2);
  tft_.drawString(l3, 160, 120, 2);
  const bool en = (st == LimitStatus::DemoSimulated);
  const char* lbl = armed ? "CLEAR DEMO LIMIT" : (en ? "HOLD 1.5s TO ARM (DEMO)" : "UNAVAILABLE");
  drawBtn(holdBtn(), lbl, en, armed ? cWarn_ : cNeon_);
  holdDrawn_ = 0;
  hold_ = HoldTracker();
}

void DisplayUI::tickSafety(bool touching, int16_t mx, int16_t my, const GestureEvent& ev, uint32_t now) {
  const bool demo = c_.store->get().demo;
  const LimitStatus st = c_.safety->status(demo);
  const bool armed = c_.safety->demoArmed();
  const uint8_t key = (uint8_t)((uint8_t)st * 2 + (armed ? 1 : 0));
  if (key != safetyKey_) { safetyKey_ = key; drawSafetyBody(st, armed); }

  const Btn hb = holdBtn();
  const bool canHold = (st == LimitStatus::DemoSimulated) && !armed;
  if (canHold) {
    const bool inside = touching && inBtn(hb, mx, my);
    if (hold_.update(inside, now, kHoldMs)) {
      c_.safety->requestQuickLimit(demo);  // local demo only, nothing is transmitted
      safetyKey_ = 255;
    } else {
      const int w = (int)(hold_.progress(now, kHoldMs) * (float)(hb.w - 8));
      if (w != holdDrawn_) {
        if (w > holdDrawn_) tft_.fillRect(hb.x + 4 + holdDrawn_, hb.y + hb.h - 8, w - holdDrawn_, 4, cNeon_);
        else tft_.fillRect(hb.x + 4 + w, hb.y + hb.h - 8, holdDrawn_ - w, 4, cPanel_);
        holdDrawn_ = w;
      }
    }
  }
  if (ev.type == GestureType::Tap) {
    if (inBtn(bottomBtn(0), ev.x, ev.y)) { go(Screen::Dash); return; }
    if (armed && inBtn(hb, ev.x, ev.y)) { c_.safety->clearDemo(); safetyKey_ = 255; }
  }
}

// ------------------------------------------------------------------ diagnostics

void DisplayUI::drawDiag() {
  drawHeader("DIAGNOSTICS");
  drawBtn(bottomBtn(0), "BACK", true, cNeon_);
  lastDiagMs_ = 0;
}

void DisplayUI::tickDiag(const GestureEvent& ev, uint32_t now) {
  if (ev.type == GestureType::Tap) {
    if (inBtn(bottomBtn(0), ev.x, ev.y)) { go(Screen::Dash); return; }
    if (ev.y >= 148 && ev.y < 202) {
      Settings& st = c_.store->get();
      BleTarget cur = c_.ble->target();
      BleTarget nt;  // empty = scan mode
      bool change = false;
      if (cur.valid()) {
        if (ev.y < 166) change = true;  // tap the target line to forget it
      } else {
        BleSeen d;
        if (c_.ble->get((uint8_t)((ev.y - 148) / 18), d)) {
          strlcpy(nt.addr, d.addr, sizeof(nt.addr));
          strlcpy(nt.name, d.name, sizeof(nt.name));
          nt.type = d.addrType;
          change = true;
        }
      }
      if (change) {
        c_.ble->setTarget(nt);
        strlcpy(st.targetAddr, nt.addr, sizeof(st.targetAddr));
        strlcpy(st.targetName, nt.name, sizeof(st.targetName));
        st.targetType = nt.type;
        c_.store->save();
        lastDiagMs_ = 0;
      }
    }
  }
  if (lastDiagMs_ != 0 && now - lastDiagMs_ < 500) return;
  lastDiagMs_ = now ? now : 1;
  char b[48];
  snprintf(b, sizeof(b), "NOVA RIDE OS v%s %s", NOVA_FW_VERSION, NOVA_PROFILE_NAME);
  txt(0, 8, 40, 2, b, cText_, TL_DATUM, 304);
  snprintf(b, sizeof(b), "Heap %u/%u  up %lus", (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
           (unsigned long)(now / 1000UL));
  txt(1, 8, 58, 2, b, cDim_, TL_DATUM, 304);
  const BleState bs = c_.ble->state();
  const char* bn = bs == BleState::Off ? "off" : bs == BleState::Connected ? "linked" : bs == BleState::Connecting ? "connecting" : "scan";
  snprintf(b, sizeof(b), "BLE %s  seen %u  reset %d", bn, (unsigned)c_.ble->count(), (int)esp_reset_reason());
  txt(2, 8, 76, 2, b, cDim_, TL_DATUM, 304);

  const BleTarget tg = c_.ble->target();
  const BleLinkInfo li = c_.ble->linkInfo();
  if (tg.valid()) snprintf(b, sizeof(b), "Target %.16s", tg.name[0] ? tg.name : tg.addr);
  else snprintf(b, sizeof(b), "No target - tap a device below");
  txt(3, 8, 94, 2, b, tg.valid() ? cText_ : cWarn_, TL_DATUM, 304);
  if (bs == BleState::Connected)
    snprintf(b, sizeof(b), "Link %s  key %02X  var %u", li.profile, (unsigned)li.key, (unsigned)li.variant);
  else if (li.refused) snprintf(b, sizeof(b), "REFUSED: variant %u not decoded", (unsigned)li.variant);
  else if (bs == BleState::Connecting) snprintf(b, sizeof(b), "%s", li.step[0] ? li.step : "Connecting...");
  else snprintf(b, sizeof(b), "Not connected (read-only build)");
  txt(4, 8, 112, 2, b, bs == BleState::Connected ? cOk_ : cDim_, TL_DATUM, 304);
  if (li.triage[0]) snprintf(b, sizeof(b), "%s", li.triage);
  else snprintf(b, sizeof(b), "ntf %lu frm %lu ver %s", (unsigned long)li.notifies, (unsigned long)li.frames,
                li.version[0] ? li.version : "--");
  txt(5, 8, 130, 2, b, li.triage[0] ? cWarn_ : cDim_, TL_DATUM, 304);

  if (tg.valid()) {
    txt(6, 8, 148, 2, "Tap here to forget target", cDim_, TL_DATUM, 304);
    txt(7, 8, 166, 2, " ", cDim_, TL_DATUM, 304);
    txt(8, 8, 184, 2, " ", cDim_, TL_DATUM, 304);
  } else {
    for (uint8_t i = 0; i < 3; ++i) {
      BleSeen d;
      if (c_.ble->get(i, d)) {
        snprintf(b, sizeof(b), "%c%-14.14s %4d", d.candidate ? '*' : ' ', d.name[0] ? d.name : d.addr, (int)d.rssi);
        txt((uint8_t)(6 + i), 8, (int16_t)(148 + i * 18), 2, b, d.candidate ? cWarn_ : cText_, TL_DATUM, 304);
      } else {
        txt((uint8_t)(6 + i), 8, (int16_t)(148 + i * 18), 2, i == 0 ? " no devices seen (BLE on?)" : " ", cDim_, TL_DATUM, 304);
      }
    }
  }
}

// ------------------------------------------------------------------ ride controls

namespace {
DisplayUI::Btn modeBtn(int i) { return mk((int16_t)(6 + i * 79), 40, 74, 46); }
DisplayUI::Btn togBtn(int i) { return mk((int16_t)(8 + i * 104), 96, 96, 46); }
}  // namespace

void DisplayUI::drawCtrl() {
  drawHeader("RIDE CONTROLS");
  const bool demo = c_.store->get().demo;
  const ScooterControl& c = *c_.ctrl;
  const char* names[4] = {"WALK", "ECO", "DRIVE", "SPORT"};
  for (int i = 0; i < 4; ++i) {
    const bool sel = demo && ((int)c.mode() == i);
    const Btn b = modeBtn(i);
    drawBtn(b, names[i], demo, sel ? cOk_ : cNeon_);
    if (sel) tft_.drawRoundRect(b.x + 2, b.y + 2, b.w - 4, b.h - 4, 5, cOk_);
  }
  char l[20];
  snprintf(l, sizeof(l), "LIGHT %s", demo ? (c.light() ? "ON" : "OFF") : "--");
  drawBtn(togBtn(0), l, demo, c.light() ? cOk_ : cNeon_);
  snprintf(l, sizeof(l), "ZERO %s", demo ? (c.zeroStart() ? "ON" : "OFF") : "--");
  drawBtn(togBtn(1), l, demo, c.zeroStart() ? cOk_ : cNeon_);
  snprintf(l, sizeof(l), "HAZARD %s", demo ? (c.hazard() ? "ON" : "OFF") : "--");
  drawBtn(togBtn(2), l, demo, c.hazard() ? cWarn_ : cNeon_);
  tft_.setTextPadding(0);
  tft_.setTextDatum(TC_DATUM);
  if (demo) {
    tft_.setTextColor(cWarn_, cBg_);
    tft_.drawString("DEMO: simulated, nothing is sent", 160, 154, 2);
    tft_.setTextColor(cDim_, cBg_);
    tft_.drawString("Scooter state: NOT CHANGED", 160, 174, 2);
  } else {
    const bool linked = c_.ble->state() == BleState::Connected;
    tft_.setTextColor(linked ? cWarn_ : cBad_, cBg_);
    tft_.drawString(linked ? "READ-ONLY: writes are off" : "NOT SUPPORTED: no scooter link", 160, 154, 2);
    tft_.setTextColor(cDim_, cBg_);
    tft_.drawString(linked ? "Nothing is sent to the scooter" : "Scooter state: UNKNOWN", 160, 174, 2);
  }
  drawBtn(bottomBtn(0), "BACK", true, cNeon_);
}

void DisplayUI::tickCtrl(const GestureEvent& ev) {
  if (ev.type != GestureType::Tap) return;
  if (inBtn(bottomBtn(0), ev.x, ev.y)) { go(Screen::Dash); return; }
  const bool demo = c_.store->get().demo;
  if (!demo) return;  // controls are inert without a verified scooter protocol
  ScooterControl& c = *c_.ctrl;
  bool changed = false;
  for (int i = 0; i < 4; ++i)
    if (inBtn(modeBtn(i), ev.x, ev.y)) { c.setMode((RideMode)i, demo); changed = true; }
  if (inBtn(togBtn(0), ev.x, ev.y)) { c.setLight(!c.light(), demo); changed = true; }
  if (inBtn(togBtn(1), ev.x, ev.y)) { c.setZeroStart(!c.zeroStart(), demo); changed = true; }
  if (inBtn(togBtn(2), ev.x, ev.y)) { c.setHazard(!c.hazard(), demo); changed = true; }
  if (changed) full_ = true;
}
