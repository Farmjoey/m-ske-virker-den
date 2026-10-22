// Host-side unit tests for the pure logic modules (no ESP32 needed).
#include <stdio.h>
#include <string.h>
#include "../../src/Format.h"
#include "../../src/Gesture.h"
#include "../../src/TouchCal.h"
#include "../../src/SafetyLogic.h"
#include "../../src/SafetyProfile.h"
#include "../../src/DemoTelemetry.h"
#include "../../src/ScooterControl.h"
#include "../../src/XbotProtocol.h"

static int g_fail = 0, g_n = 0;
#define CHECK(c) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// Stubs for the two functions the real firmware implements in ScooterProtocol.cpp
namespace ScooterProtocol {
bool parseNotification(const uint8_t*, size_t, Telemetry&) { return false; }
bool requestSpeedLimit(uint8_t) { return false; }
bool writeMode(uint8_t) { return false; }
bool writeLight(bool) { return false; }
bool writeZeroStart(bool) { return false; }
bool writeHazard(bool) { return false; }
}

static GestureEvent drag(GestureDetector& g, int x0, int y0, int x1, int y1, uint32_t ms) {
  uint32_t t = 1000;
  g.update(true, (int16_t)x0, (int16_t)y0, t);
  g.update(true, (int16_t)((x0 + x1) / 2), (int16_t)((y0 + y1) / 2), t + ms / 2);
  g.update(true, (int16_t)x1, (int16_t)y1, t + ms - 1);
  return g.update(false, 0, 0, t + ms);
}

static void test_format() {
  char b[16];
  fmtFixed(b, sizeof b, false, 12.3f, 1); CHECK(strcmp(b, "--") == 0);   // never invent values
  fmtFixed(b, sizeof b, true, 24.6f, 0);  CHECK(strcmp(b, "25") == 0);
  fmtFixed(b, sizeof b, true, 37.24f, 1); CHECK(strcmp(b, "37.2") == 0);
}

static void test_telemetry_default_invalid() {
  Telemetry t;
  CHECK(!t.speedValid && !t.battPctValid && !t.voltValid && !t.powerValid);
  CHECK(!t.tripValid && !t.totalValid && !t.faultValid && !t.modeValid && !t.isDemo);
}

static void test_gestures() {
  GestureDetector g;
  GestureEvent e = drag(g, 160, 180, 165, 90, 250);
  CHECK(e.type == GestureType::SwipeUp);
  e = drag(g, 160, 80, 158, 170, 250);   CHECK(e.type == GestureType::SwipeDown);
  e = drag(g, 250, 100, 100, 110, 250);  CHECK(e.type == GestureType::SwipeLeft);
  e = drag(g, 100, 100, 250, 105, 250);  CHECK(e.type == GestureType::SwipeRight);
  e = drag(g, 100, 100, 104, 103, 120);  CHECK(e.type == GestureType::Tap);
  CHECK(e.x == 100 && e.y == 100);
  e = drag(g, 160, 180, 165, 90, 1500);  CHECK(e.type == GestureType::None);   // too slow
  e = drag(g, 160, 180, 195, 150, 250);  CHECK(e.type == GestureType::None);   // diagonal / short
  e = g.update(false, 0, 0, 5000);       CHECK(e.type == GestureType::None);   // release w/o press
}

static void test_hold() {
  HoldTracker h;
  CHECK(!h.update(true, 1000, 1500));
  CHECK(!h.update(true, 2000, 1500));
  CHECK(h.update(true, 2500, 1500));     // fires once
  CHECK(!h.update(true, 2600, 1500));
  CHECK(!h.update(false, 2700, 1500));   // released
  CHECK(!h.update(true, 2800, 1500));    // restarts
  CHECK(h.progress(3300, 1500) > 0.3f && h.progress(3300, 1500) < 0.4f);
  h.update(false, 3400, 1500);
  CHECK(h.progress(3500, 1500) == 0.0f);
}

static RawPt rawFromScreen(float sx, float sy, bool swap, bool flipX, bool flipY) {
  float a = flipX ? (3800.0f - sx * 11.0f) : (200.0f + sx * 11.0f);   // along screen X
  float b = flipY ? (3800.0f - sy * 15.0f) : (200.0f + sy * 15.0f);   // along screen Y
  RawPt p;
  if (swap) { p.x = (int16_t)b; p.y = (int16_t)a; } else { p.x = (int16_t)a; p.y = (int16_t)b; }
  return p;
}

static void test_cal_variant(bool swap, bool fx, bool fy) {
  TouchCal c;
  RawPt tl = rawFromScreen(20, 20, swap, fx, fy);
  RawPt tr = rawFromScreen(300, 20, swap, fx, fy);
  RawPt br = rawFromScreen(300, 220, swap, fx, fy);
  CHECK(TouchCal::compute(tl, tr, br, 320, 240, 20, c));
  CHECK(c.swapXY == swap);
  const float pts[4][2] = {{20, 20}, {300, 220}, {160, 120}, {60, 200}};
  for (int i = 0; i < 4; ++i) {
    RawPt r = rawFromScreen(pts[i][0], pts[i][1], swap, fx, fy);
    int16_t sx, sy; c.map(r.x, r.y, 320, 240, sx, sy);
    CHECK(abs(sx - (int)pts[i][0]) <= 2 && abs(sy - (int)pts[i][1]) <= 2);
  }
  int16_t sx, sy; c.map(0, 0, 320, 240, sx, sy);
  CHECK(sx >= 0 && sx < 320 && sy >= 0 && sy < 240);   // clamped
}

static void test_cal() {
  for (int s = 0; s < 2; ++s) for (int x = 0; x < 2; ++x) for (int y = 0; y < 2; ++y)
    test_cal_variant(s != 0, x != 0, y != 0);
  TouchCal bad; RawPt same = {1000, 1000};
  CHECK(!TouchCal::compute(same, same, same, 320, 240, 20, bad));  // implausible input rejected
  CHECK(!bad.valid);
}

static void test_safety() {
  SafetyInputs in;
  in.protocolVerified = false; in.linkUp = true; in.ackReceived = true; in.demoMode = false;
  CHECK(computeLimitStatus(in) == LimitStatus::NotSupported);   // never Confirmed w/o verified protocol
  in.demoMode = true;
  CHECK(computeLimitStatus(in) == LimitStatus::DemoSimulated);
  in.protocolVerified = true; in.linkUp = false; in.ackReceived = false;
  CHECK(computeLimitStatus(in) == LimitStatus::Unknown);        // link error -> unknown
  in.linkUp = true;
  CHECK(computeLimitStatus(in) == LimitStatus::Unknown);        // no ack -> unknown
  in.ackReceived = true;
  CHECK(computeLimitStatus(in) == LimitStatus::Confirmed);

  CHECK(mayRequestLimit(20, false, 0));
  CHECK(mayRequestLimit(20, true, 25));
  CHECK(mayRequestLimit(20, true, 20));
  CHECK(!mayRequestLimit(20, true, 15));   // would raise the limit
  CHECK(!mayRequestLimit(25, true, 30));   // only 20 allowed
  CHECK(!mayRequestLimit(0, false, 0));

  SafetyProfile sp;
  CHECK(sp.status(false) == LimitStatus::NotSupported);
  CHECK(sp.requestQuickLimit(false) == QuickResult::NotSupported && !sp.demoArmed());
  CHECK(sp.status(true) == LimitStatus::DemoSimulated);
  CHECK(sp.requestQuickLimit(true) == QuickResult::DemoOnly && sp.demoArmed());
  sp.clearDemo(); CHECK(!sp.demoArmed());
  sp.requestQuickLimit(true);
  CHECK(sp.requestQuickLimit(false) == QuickResult::NotSupported && !sp.demoArmed());
  CHECK(!ScooterProtocol::kVerified && !ScooterProtocol::kWriteSupported);
  CHECK(!ScooterProtocol::requestSpeedLimit(20));                // nothing is ever sent
}

static void test_demo() {
  for (uint32_t t = 0; t < 600000; t += 777) {
    Telemetry d = demoSample(t, 0);
    CHECK(d.isDemo && d.speedValid && d.speedKmh >= 0.0f && d.speedKmh <= 31.0f);
    Telemetry c = demoSample(t, 20);
    CHECK(c.speedKmh <= 20.001f && c.isDemo);                    // demo cap works
    CHECK(c.battPct >= 30 && c.battPct <= 100);
  }
}

static void test_control() {
  ScooterControl c;
  CHECK(c.mode() == RideMode::Eco && !c.light() && !c.zeroStart() && !c.hazard());
  // demo: local state changes, nothing "sent"
  CHECK(c.setMode(RideMode::Sport, true) == ControlResult::DemoOnly && c.mode() == RideMode::Sport);
  CHECK(c.setLight(true, true) == ControlResult::DemoOnly && c.light());
  CHECK(c.setZeroStart(true, true) == ControlResult::DemoOnly && c.zeroStart());
  CHECK(c.setHazard(true, true) == ControlResult::DemoOnly && c.hazard());
  // not demo: never sent, state never changes
  CHECK(c.setMode(RideMode::Walk, false) == ControlResult::NotSupported && c.mode() == RideMode::Sport);
  CHECK(c.setLight(false, false) == ControlResult::NotSupported && c.light());
  CHECK(c.setZeroStart(false, false) == ControlResult::NotSupported && c.zeroStart());
  CHECK(c.setHazard(false, false) == ControlResult::NotSupported && c.hazard());
  CHECK(demoModeCapKmh(RideMode::Walk) < demoModeCapKmh(RideMode::Eco));
  CHECK(demoModeCapKmh(RideMode::Eco) < demoModeCapKmh(RideMode::Drive));
  CHECK(demoModeCapKmh(RideMode::Drive) < demoModeCapKmh(RideMode::Sport));
  CHECK(strcmp(rideModeName(RideMode::Walk), "WALK") == 0 && strcmp(rideModeName(RideMode::Sport), "SPORT") == 0);
}

// ---- XBOT protocol -------------------------------------------------------------------
// Real frame captured off a 36 V vehicle (de-obfuscated), taken from the esphome-xbot tests.
static const uint8_t kRealReply[] = {0x55, 0xAA, 0x22, 0x23, 0x01, 0xEA, 0x00, 0x00, 0x00, 0x00,
                                     0x01, 0x00, 0x00, 0x00, 0xF5, 0x00, 0x4F, 0x02, 0x4F, 0x02,
                                     0x62, 0x01, 0x00, 0x00, 0x88, 0x13, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x8D, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAC, 0xFB};

static void expectVariant(const char* name, int id, int key, int dest, int cmd) {
  xbot::ProtocolVariant v = xbot::variantForName(name);
  CHECK(v.id == id && v.xorKey == key && v.readDest == dest && v.readCmd == cmd);
}

static void test_xbot_variants() {
  expectVariant("M0Robot", 3, 0x34, 0x20, 0x61);
  expectVariant("M0Robot_with_a_suffix", 3, 0x34, 0x20, 0x61);
  expectVariant("", 3, 0x34, 0x20, 0x61);
  expectVariant("something else entirely", 3, 0x34, 0x20, 0x61);   // fallback = variant 3
  expectVariant("X0Robot123", 3, 0x00, 0x20, 0x61);
  expectVariant("MIScooter", 3, 0x00, 0x20, 0x61);
  expectVariant("MiniRobot", 1, 0x55, 0x0A, 0x01);
  expectVariant("M5Robot", 1, 0xD8, 0x0A, 0x01);
  expectVariant("A6Robot", 2, 0xD8, 0x0A, 0x01);
  expectVariant("W1", 5, 0x37, 0x0A, 0x01);
  expectVariant("GoKart", 4, 0x00, 0x0A, 0x01);
  expectVariant("M0Clean77", 6, 0x00, 0x32, 0x01);
  expectVariant("TK2-S99", 3, 0x34, 0x20, 0x61);
  expectVariant("TK9000", 7, 0x00, 0x0A, 0x01);
  expectVariant("miniPLUS_9", 1, 0x00, 0x04, 0x01);
  expectVariant("ninebot", 3, 0x34, 0x20, 0x61);                    // case-sensitive
  expectVariant("Ninebot", 1, 0x00, 0x0A, 0x01);
  expectVariant("M1abcdef", 1, 0x7A, 0x0A, 0x01);
  expectVariant("M1abcd", 3, 0x34, 0x20, 0x61);
  expectVariant("XYabcde", 1, 0x18, 0x0A, 0x01);
  expectVariant("aXbcdefg", 1, 0x18, 0x0A, 0x01);
  expectVariant("XYabcd", 3, 0x34, 0x20, 0x61);
  expectVariant("abXcdefg", 3, 0x34, 0x20, 0x61);
}

static void test_xbot_framing() {
  uint8_t b[xbot::kMaxRequest];
  const uint8_t one78[] = {0x78};
  size_t n = xbot::buildRequest(b, sizeof b, 0x20, 0x61, 0x19, one78, 1);
  const uint8_t want[] = {0x55, 0xAA, 0x03, 0x20, 0x61, 0x19, 0x78, 0xEA, 0xFE};
  CHECK(n == sizeof(want) && memcmp(b, want, n) == 0);
  const uint8_t one1e[] = {0x1E};
  n = xbot::buildRequest(b, sizeof b, 0x21, 0x01, 0x14, one1e, 1);
  const uint8_t want2[] = {0x55, 0xAA, 0x03, 0x21, 0x01, 0x14, 0x1E, 0xA8, 0xFF};
  CHECK(n == sizeof(want2) && memcmp(b, want2, n) == 0);
  CHECK(xbot::checksumOk(b, n));
  b[6] ^= 1; CHECK(!xbot::checksumOk(b, n));
  CHECK(xbot::buildRequest(b, 4, 0x20, 0x61, 0, one78, 1) == 0);   // buffer too small
  // every poll entry builds a valid frame
  for (int i = 0; i < xbot::kPollCount; ++i) {
    const xbot::PollEntry& p = xbot::kPollTable[i];
    uint8_t cnt[1] = {p.count};
    size_t m = xbot::buildRequest(b, sizeof b, p.dest, p.cmd, p.reg, cnt, 1);
    CHECK(m == 9 && xbot::checksumOk(b, m));
  }
  // xor is symmetric and covers the whole frame
  uint8_t f[sizeof(kRealReply)]; memcpy(f, kRealReply, sizeof f);
  xbot::applyXor(f, sizeof f, 0x34);
  CHECK(f[0] == (0x55 ^ 0x34) && f[1] == (0xAA ^ 0x34));
  CHECK(xbot::diagnoseKey(f, sizeof f) == 0x34);
  CHECK(xbot::diagnoseKey(kRealReply, sizeof kRealReply) == 0x00);
  const uint8_t junk[] = {0x01, 0x02, 0x03};
  CHECK(xbot::diagnoseKey(junk, 3) == -1);
  xbot::applyXor(f, sizeof f, 0x34);
  CHECK(memcmp(f, kRealReply, sizeof f) == 0);
  // live sequence only references valid table entries
  for (int i = 0; i < xbot::kLiveCount; ++i) CHECK(xbot::kLiveSeq[i] < xbot::kPollCount);
}

static void test_xbot_real_frame() {
  CHECK(xbot::checksumOk(kRealReply, sizeof kRealReply));
  xbot::RxAccumulator<256> rx;
  int frames = 0;
  uint8_t keyed[sizeof(kRealReply)]; memcpy(keyed, kRealReply, sizeof keyed);
  xbot::applyXor(keyed, sizeof keyed, 0x34);
  rx.append(keyed, 17, 0x34);                         // split across two notifications
  rx.drain([&](const uint8_t*, size_t) { ++frames; });
  CHECK(frames == 0 && rx.size() == 17);
  rx.append(keyed + 17, sizeof keyed - 17, 0x34);
  xbot::RegisterFile rf;
  rx.drain([&](const uint8_t* f, size_t n) {
    ++frames; rf.beginFrame();
    xbot::decodeRegisters(f, n, [&](uint8_t r, uint16_t v) { rf.set(r, v); });
  });
  CHECK(frames == 1 && rx.size() == 0);
  CHECK(rf.get(0xEE) == 245 && rf.get(0xEF) == 591 && rf.get(0xF0) == 591 && rf.get(0xF1) == 354);
  CHECK(rf.get(0xF2) == 0 && rf.get(0xF3) == 5000 && rf.get(0xF7) == 141 && rf.get(0xEC) == 1);
  CHECK(rf.has(0xEA) && rf.has(0xF9) && !rf.has(0xFA) && !rf.has(0xE9));
  // junk before a header and a corrupt frame must not stall the next good one
  uint8_t stream[sizeof(kRealReply) * 2 + 4]; size_t k = 0;
  stream[k++] = 0x00; stream[k++] = 0x13;
  memcpy(stream + k, kRealReply, sizeof kRealReply); k += sizeof kRealReply;
  stream[k - 3] ^= 0xFF;                               // corrupt checksum/payload
  memcpy(stream + k, kRealReply, sizeof kRealReply); k += sizeof kRealReply;
  int good = 0;
  xbot::extractFrames(stream, k, [&](const uint8_t*, size_t) { ++good; });
  CHECK(good == 1);
}

// Builds a reply frame (src 0x23) carrying consecutive registers from `base`.
static size_t makeReply(uint8_t* out, size_t cap, uint8_t base, const uint16_t* regs, int n) {
  uint8_t payload[64];
  for (int i = 0; i < n; ++i) { payload[2 * i] = (uint8_t)(regs[i] & 0xFF); payload[2 * i + 1] = (uint8_t)(regs[i] >> 8); }
  return xbot::buildRequest(out, cap, 0x23, 0x01, base, payload, (size_t)(2 * n));
}

static void feed(xbot::RegisterFile& rf, uint8_t base, const uint16_t* regs, int n) {
  uint8_t f[80]; size_t len = makeReply(f, sizeof f, base, regs, n);
  CHECK(len > 0 && xbot::checksumOk(f, len));
  rf.beginFrame();
  xbot::decodeRegisters(f, len, [&](uint8_t r, uint16_t v) { rf.set(r, v); });
}

// SYNTHETIC values (not from a real vehicle) to test the register -> dashboard mapping.
static void test_xbot_telemetry() {
  xbot::RegisterFile rf;
  Telemetry t;
  xbot::fillTelemetry(rf, t);
  CHECK(!t.speedValid && !t.battPctValid && !t.voltValid && !t.powerValid && !t.tripValid && !t.totalValid);
  CHECK(!t.faultValid && !t.modeValid && !t.isDemo);                       // nothing read -> all "--"

  const uint16_t a[] = {77, 0, 0, 0, 0, 1234, 0, 0x5678, 0x0001};              // 0x22..0x2A
  feed(rf, 0x22, a, 9);
  const uint16_t trip[] = {150};   feed(rf, 0x2F, trip, 1);
  const uint16_t vi[] = {4812, 250}; feed(rf, 0x47, vi, 2);                      // 0x47,0x48 same frame
  const uint16_t err[] = {0};      feed(rf, 0x1B, err, 1);
  const uint16_t mode[] = {2};     feed(rf, 0x7E, mode, 1);
  xbot::fillTelemetry(rf, t);
  CHECK(t.battPctValid && t.battPct == 77);
  CHECK(t.speedValid && t.speedKmh > 12.33f && t.speedKmh < 12.35f);
  CHECK(t.totalValid && t.totalKm > 87.67f && t.totalKm < 87.68f);            // 0x15678 / 1000
  CHECK(t.tripValid && t.tripKm > 1.49f && t.tripKm < 1.51f);
  CHECK(t.voltValid && t.volts > 48.11f && t.volts < 48.13f);
  CHECK(t.powerValid && t.powerW == 120);                                       // 48.12 V * 2.50 A
  CHECK(t.faultValid && t.faultCode == 0);
  CHECK(t.modeValid && strcmp(t.mode, "#2") == 0 && !t.isDemo);

  const uint16_t na[] = {0xFFFF, 0, 0, 0, 0, 0xFFFF};                           // not available markers
  feed(rf, 0x22, na, 6);
  xbot::fillTelemetry(rf, t);
  CHECK(!t.battPctValid && !t.speedValid);                                       // never shown as numbers
  // 32-bit total needs both words from one frame; a stale high word must not be paired
  const uint16_t lo[] = {0x1111}; feed(rf, 0x29, lo, 1);
  xbot::fillTelemetry(rf, t);
  CHECK(!t.totalValid);
  // a negative current (regeneration) is valid and gives negative power
  const uint16_t reg[] = {4800, 0xFFFE}; feed(rf, 0x47, reg, 2);                 // -0.02 A
  xbot::fillTelemetry(rf, t);
  CHECK(t.powerValid && t.powerW <= 0);
  // out-of-range reading is dropped, not clamped
  const uint16_t big[] = {20000}; feed(rf, 0x47, big, 1);                         // 200 V
  xbot::fillTelemetry(rf, t);
  CHECK(!t.voltValid);
  const uint16_t badmode[] = {9}; feed(rf, 0x7E, badmode, 1);
  xbot::fillTelemetry(rf, t);
  CHECK(!t.modeValid);
  char v[12];
  CHECK(!xbot::controllerVersion(rf, v, sizeof v));                              // not read yet
  const uint16_t ver[] = {0x0605, 0x1153}; feed(rf, 0x1D, ver, 2);               // 0x1D, 0x1E
  CHECK(xbot::controllerVersion(rf, v, sizeof v));
  CHECK(strcmp(v, "11530605") == 0);                                             // 0x1E first, then 0x1D
}

int main() {
  test_format(); test_telemetry_default_invalid(); test_gestures(); test_hold();
  test_cal(); test_safety(); test_demo(); test_control();
  test_xbot_variants(); test_xbot_framing(); test_xbot_real_frame(); test_xbot_telemetry();
  printf("%d checks, %d failed\n", g_n, g_fail);
  return g_fail ? 1 : 0;
}
