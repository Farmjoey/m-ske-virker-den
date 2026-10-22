#include "ScooterBLE.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>
#include <ctype.h>
#include "BoardConfig.h"
#include "Diagnostics.h"
#include "XbotProtocol.h"

namespace {

// ---------------------------------------------------------------- shared state
struct Entry { BleSeen seen; uint32_t lastMs; bool used; };
const uint8_t kMax = 8;
const uint32_t kFreshMs = 30000;
const uint32_t kStaleMs = 5000;       // telemetry older than this is not shown
const uint32_t kProbeGapMs = 300;     // spacing between read requests (as in the reference)
const uint32_t kSettleMs = 800;

enum Phase : uint8_t { PhIdle, PhConnecting, PhLinked };

Entry s_tab[kMax];
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
NimBLEScan* s_scan = nullptr;
bool s_inited = false;

volatile bool s_enabled = false;
volatile Phase s_phase = PhIdle;
volatile bool s_targetChanged = false;
volatile bool s_linkDown = false;
BleTarget s_target;
bool s_refused = false;

xbot::RegisterFile s_regs;
xbot::RxAccumulator<256> s_rx;
BleLinkInfo s_info;
uint8_t s_key = 0;
uint32_t s_lastFrame = 0;
uint8_t s_first[8];
size_t s_firstLen = 0;

NimBLEClient* s_client = nullptr;
NimBLERemoteCharacteristic* s_write = nullptr;
bool s_writeResp = false;
uint32_t s_linkStart = 0, s_nextProbe = 0, s_lastTriage = 0;
int s_sweepIdx = 0, s_liveIdx = 0;

bool nameHint(const char* name) {
  char low[20];
  size_t i = 0;
  for (; name[i] && i < sizeof(low) - 1; ++i) low[i] = (char)tolower((unsigned char)name[i]);
  low[i] = 0;
  for (int h = 0; h < board::kScooterNameHintCount; ++h)
    if (strstr(low, board::kScooterNameHints[h])) return true;
  return false;
}

class ScanCb : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* dev) override {
    BleSeen s;
    strlcpy(s.name, dev->getName().c_str(), sizeof(s.name));
    strlcpy(s.addr, dev->getAddress().toString().c_str(), sizeof(s.addr));
    s.rssi = (int8_t)dev->getRSSI();
    s.addrType = (uint8_t)dev->getAddress().getType();
    s.candidate = nameHint(s.name);
    const uint32_t now = millis();
    portENTER_CRITICAL(&s_mux);
    int slot = -1, oldest = 0;
    for (int i = 0; i < kMax; ++i) {
      if (s_tab[i].used && strcmp(s_tab[i].seen.addr, s.addr) == 0) { slot = i; break; }
      if (!s_tab[i].used) { if (slot < 0) slot = i; }
      else if (s_tab[i].lastMs < s_tab[oldest].lastMs) oldest = i;
    }
    if (slot < 0) slot = oldest;
    s_tab[slot].seen = s;
    s_tab[slot].lastMs = now;
    s_tab[slot].used = true;
    portEXIT_CRITICAL(&s_mux);
  }
};
ScanCb s_scanCb;

class ClientCb : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient*) override {}
  void onDisconnect(NimBLEClient*) override { s_linkDown = true; }
};
ClientCb s_clientCb;

// ---------------------------------------------------------------- notifications
void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  portENTER_CRITICAL(&s_mux);
  s_info.notifies++;
  if (s_firstLen == 0) {
    s_firstLen = len < sizeof(s_first) ? len : sizeof(s_first);
    memcpy(s_first, data, s_firstLen);
  }
  s_rx.append(data, len, s_key);
  s_rx.drain([](const uint8_t* f, size_t n) {
    s_info.frames++;
    if (f[3] == xbot::kSrcMain) {
      s_regs.beginFrame();
      xbot::decodeRegisters(f, n, [](uint8_t r, uint16_t v) { s_regs.set(r, v); });
      s_lastFrame = millis();
    }
  });
  portEXIT_CRITICAL(&s_mux);
}

// ---------------------------------------------------------------- task helpers
void ensureInit() {
  if (s_inited) return;
  NimBLEDevice::init("NOVA-RIDE");
  s_scan = NimBLEDevice::getScan();
  s_scan->setAdvertisedDeviceCallbacks(&s_scanCb, false);
  s_scan->setActiveScan(true);
  s_scan->setInterval(160);
  s_scan->setWindow(80);
  s_scan->setMaxResults(0);
  s_inited = true;
  Diag::log("BLE init (read-only)");
}

void dropLink() {
  if (s_client && s_client->isConnected()) s_client->disconnect();
  portENTER_CRITICAL(&s_mux);
  s_phase = PhIdle;
  s_write = nullptr;
  s_regs.clear();
  s_rx.clear();
  s_lastFrame = 0;
  portEXIT_CRITICAL(&s_mux);
  s_linkDown = false;
}

// Waits up to `ms`, returns early when the user changes the target or disables BLE.
void waitMs(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 100) {
    if (!s_enabled || s_targetChanged) return;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setStep(const char* fmt, const char* a, unsigned b) {
  portENTER_CRITICAL(&s_mux);
  snprintf(s_info.step, sizeof(s_info.step), fmt, a, b);
  portEXIT_CRITICAL(&s_mux);
}

// One documented READ request with the given key.
bool sendPoll(NimBLERemoteCharacteristic* w, bool resp, int idx, uint8_t key) {
  const xbot::PollEntry& p = xbot::kPollTable[idx];
  uint8_t buf[xbot::kMaxRequest];
  const uint8_t cnt[1] = {p.count};
  const size_t len = xbot::buildRequest(buf, sizeof(buf), p.dest, p.cmd, p.reg, cnt, 1);
  xbot::applyXor(buf, len, key);
  return len != 0 && w->writeValue(buf, len, resp);
}

uint32_t framesSeen() {
  uint32_t f;
  portENTER_CRITICAL(&s_mux);
  f = s_info.frames;
  portEXIT_CRITICAL(&s_mux);
  return f;
}

bool connectAndSetup(const BleTarget& tg) {
  s_phase = PhConnecting;
  if (!s_client) {
    s_client = NimBLEDevice::createClient();
    s_client->setClientCallbacks(&s_clientCb, false);
    s_client->setConnectTimeout(8);
  }
  Diag::log("BLE connect %s", tg.addr);
  NimBLEAddress addr(std::string(tg.addr), tg.type);
  if (!s_client->connect(addr)) {
    Diag::log("connect failed");
    s_phase = PhIdle;
    return false;
  }

  // Variant (and with it the obfuscation key) follows from the device name, exactly as in
  // the reference component: GATT device name first, advertised name as fallback.
  std::string nm;
  NimBLERemoteService* gap = s_client->getService("1800");
  if (gap) {
    NimBLERemoteCharacteristic* c = gap->getCharacteristic("2A00");
    if (c && c->canRead()) nm = c->readValue();
  }
  if (nm.empty()) nm = tg.name;
  const xbot::ProtocolVariant v = xbot::variantForName(nm.c_str());

  portENTER_CRITICAL(&s_mux);
  s_info = BleLinkInfo();
  strlcpy(s_info.name, nm.c_str(), sizeof(s_info.name));
  s_info.variant = v.id;
  s_info.key = v.xorKey;
  portEXIT_CRITICAL(&s_mux);

  if (v.id != xbot::kSupportedVariant) {
    s_refused = true;
    portENTER_CRITICAL(&s_mux);
    s_info.refused = true;
    portEXIT_CRITICAL(&s_mux);
    Diag::log("variant %u not decoded, nothing sent", (unsigned)v.id);
    s_client->disconnect();
    s_phase = PhIdle;
    return false;
  }

  // AUTO-DETECT. Candidates = every documented profile whose write AND notify characteristics
  // exist on this device. Each candidate is tested with each documented key for this variant
  // (name-derived key first, then the other variant-3 key). A combination is accepted ONLY when
  // the scooter answers with a frame that has the 55 AA header and a valid checksum.
  // Only READ requests are sent while testing.
  struct Cand { NimBLERemoteCharacteristic* w; NimBLERemoteCharacteristic* n; int prof; };
  Cand cands[xbot::kProfileCount];
  int nc = 0;
  for (int i = 0; i < xbot::kProfileCount; ++i) {
    NimBLERemoteService* svc = s_client->getService(xbot::kProfiles[i].service);
    if (!svc) continue;
    NimBLERemoteCharacteristic* cw = svc->getCharacteristic(xbot::kProfiles[i].write);
    NimBLERemoteCharacteristic* cn = svc->getCharacteristic(xbot::kProfiles[i].notify);
    if (cw && cn && cn->canNotify() && (cw->canWrite() || cw->canWriteNoResponse())) {
      cands[nc].w = cw; cands[nc].n = cn; cands[nc].prof = i; ++nc;
    }
  }
  if (nc == 0) {
    Diag::log("no known transport profile");
    s_client->disconnect();
    s_phase = PhIdle;
    return false;
  }
  const uint8_t altKey = (v.xorKey == 0x34) ? 0x00 : 0x34;
  const uint8_t keys[2] = {v.xorKey, altKey};
  static const int kProbeIdx[3] = {2, 1, 0};  // blocks 0x1A.., 0x46.., 0xEE..

  bool locked = false;
  for (int ci = 0; ci < nc && !locked; ++ci) {
    const xbot::TransportProfile& pr = xbot::kProfiles[cands[ci].prof];
    const bool resp = !cands[ci].w->canWriteNoResponse();
    portENTER_CRITICAL(&s_mux);
    s_rx.clear();
    s_firstLen = 0;
    s_info.notifies = 0;
    s_info.frames = 0;
    portEXIT_CRITICAL(&s_mux);
    if (!cands[ci].n->subscribe(true, onNotify)) continue;  // writes the CCCD (01 00)
    for (int ki = 0; ki < 2 && !locked; ++ki) {
      setStep("test %s key %02X", pr.name, keys[ki]);
      portENTER_CRITICAL(&s_mux);
      s_key = keys[ki];
      s_rx.clear();
      portEXIT_CRITICAL(&s_mux);
      for (int pi = 0; pi < 3 && !locked; ++pi) {
        if (!s_client->isConnected() || !s_enabled || s_targetChanged) break;
        sendPoll(cands[ci].w, resp, kProbeIdx[pi], keys[ki]);
        for (int w = 0; w < 10; ++w) {  // up to ~500 ms for an answer
          vTaskDelay(pdMS_TO_TICKS(50));
          if (framesSeen() > 0) { locked = true; break; }
        }
      }
      if (locked) {
        s_write = cands[ci].w;
        s_writeResp = resp;
        portENTER_CRITICAL(&s_mux);
        strlcpy(s_info.profile, pr.name, sizeof(s_info.profile));
        s_info.key = keys[ki];
        s_info.triage[0] = 0;
        portEXIT_CRITICAL(&s_mux);
        Diag::log("detected %s key %02X", pr.name, (unsigned)keys[ki]);
      }
    }
    if (!locked) cands[ci].n->unsubscribe();
  }

  if (!locked) {
    // No valid frame under any documented combination. Say why, then retry later.
    portENTER_CRITICAL(&s_mux);
    char tri[sizeof(s_info.triage)];
    if (s_info.notifies == 0) snprintf(tri, sizeof(tri), "no reply to read requests");
    else {
      const int k = xbot::diagnoseKey(s_first, s_firstLen);
      if (k < 0) snprintf(tri, sizeof(tri), "reply, unknown framing");
      else snprintf(tri, sizeof(tri), "wire key %02X, not tried", k);
    }
    strlcpy(s_info.triage, tri, sizeof(s_info.triage));
    s_info.step[0] = 0;
    portEXIT_CRITICAL(&s_mux);
    Diag::log("auto-detect: no valid reply");
    s_client->disconnect();
    s_phase = PhIdle;
    return false;
  }

  portENTER_CRITICAL(&s_mux);
  s_info.step[0] = 0;
  portEXIT_CRITICAL(&s_mux);
  s_linkStart = millis();
  s_nextProbe = s_linkStart + kProbeGapMs;
  s_sweepIdx = 0;
  s_liveIdx = 0;
  s_lastTriage = 0;
  s_linkDown = false;
  s_phase = PhLinked;
  return true;
}

void pollStep() {
  const uint32_t now = millis();
  if (now < s_nextProbe) { vTaskDelay(pdMS_TO_TICKS(10)); return; }
  int idx;
  if (s_sweepIdx < xbot::kPollCount) idx = s_sweepIdx++;
  else idx = xbot::kLiveSeq[s_liveIdx++ % xbot::kLiveCount];
  const xbot::PollEntry& p = xbot::kPollTable[idx];
  uint8_t buf[xbot::kMaxRequest];
  const uint8_t cnt[1] = {p.count};
  const size_t len = xbot::buildRequest(buf, sizeof(buf), p.dest, p.cmd, p.reg, cnt, 1);  // READ request
  xbot::applyXor(buf, len, s_key);
  if (s_write && len && !s_write->writeValue(buf, len, s_writeResp)) Diag::log("probe write failed");
  s_nextProbe = now + kProbeGapMs;

  // Diagnosis only: bytes arrived but nothing assembled into frames.
  if (now - s_linkStart > 6000 && now - s_lastTriage > 2000) {
    s_lastTriage = now;
    portENTER_CRITICAL(&s_mux);
    char tri[sizeof(s_info.triage)] = "";
    if (s_info.frames == 0) {
      if (s_info.notifies == 0) snprintf(tri, sizeof(tri), "no notifications yet");
      else {
        const int k = xbot::diagnoseKey(s_first, s_firstLen);
        if (k < 0) snprintf(tri, sizeof(tri), "unknown framing");
        else snprintf(tri, sizeof(tri), "wire key %02X, using %02X", k, (unsigned)s_key);
      }
    }
    strlcpy(s_info.triage, tri, sizeof(s_info.triage));
    portEXIT_CRITICAL(&s_mux);
  }
}

void scanStep() {
  if (!s_scan->isScanning()) {
    s_scan->clearResults();
    s_scan->start(4, nullptr, false);
  }
  vTaskDelay(pdMS_TO_TICKS(200));
}

void taskStep() {
  if (!s_enabled) {
    if (s_inited && s_scan && s_scan->isScanning()) s_scan->stop();
    if (s_phase != PhIdle || (s_client && s_client->isConnected())) dropLink();
    vTaskDelay(pdMS_TO_TICKS(200));
    return;
  }
  ensureInit();
  BleTarget tg;
  portENTER_CRITICAL(&s_mux);
  tg = s_target;
  portEXIT_CRITICAL(&s_mux);
  if (s_targetChanged) { s_targetChanged = false; dropLink(); }

  if (!tg.valid()) {
    if (s_phase != PhIdle || (s_client && s_client->isConnected())) dropLink();
    scanStep();
    return;
  }
  if (s_refused) { vTaskDelay(pdMS_TO_TICKS(300)); return; }
  if (s_scan->isScanning()) s_scan->stop();

  if (s_phase != PhLinked) {
    if (!connectAndSetup(tg) && !s_refused) waitMs(3000);
    return;
  }
  if (s_linkDown || !s_client || !s_client->isConnected()) {
    Diag::log("link lost");
    dropLink();
    waitMs(1000);
    return;
  }
  pollStep();
}

void taskEntry(void*) {
  for (;;) taskStep();
}

}  // namespace

// ---------------------------------------------------------------- public API
void ScooterBLE::begin() {
  xTaskCreatePinnedToCore(taskEntry, "nova_ble", 8192, nullptr, 2, nullptr, 0);
}

void ScooterBLE::setEnabled(bool on) { s_enabled = on; }
bool ScooterBLE::enabled() const { return s_enabled; }

void ScooterBLE::setTarget(const BleTarget& t) {
  portENTER_CRITICAL(&s_mux);
  s_target = t;
  s_refused = false;
  portEXIT_CRITICAL(&s_mux);
  s_targetChanged = true;
}

BleTarget ScooterBLE::target() const {
  BleTarget t;
  portENTER_CRITICAL(&s_mux);
  t = s_target;
  portEXIT_CRITICAL(&s_mux);
  return t;
}

uint8_t ScooterBLE::count() const {
  const uint32_t now = millis();
  uint8_t n = 0;
  portENTER_CRITICAL(&s_mux);
  for (int i = 0; i < kMax; ++i)
    if (s_tab[i].used && now - s_tab[i].lastMs < kFreshMs) ++n;
  portEXIT_CRITICAL(&s_mux);
  return n;
}

bool ScooterBLE::get(uint8_t idx, BleSeen& out) const {
  const uint32_t now = millis();
  bool found = false;
  uint8_t n = 0;
  portENTER_CRITICAL(&s_mux);
  for (int i = 0; i < kMax; ++i) {
    if (!s_tab[i].used || now - s_tab[i].lastMs >= kFreshMs) continue;
    if (n++ == idx) { out = s_tab[i].seen; found = true; break; }
  }
  portEXIT_CRITICAL(&s_mux);
  return found;
}

BleState ScooterBLE::state() const {
  if (!s_enabled) return BleState::Off;
  if (s_phase == PhLinked) return BleState::Connected;
  if (s_phase == PhConnecting) return BleState::Connecting;
  const uint32_t now = millis();
  bool cand = false;
  portENTER_CRITICAL(&s_mux);
  for (int i = 0; i < kMax; ++i)
    if (s_tab[i].used && s_tab[i].seen.candidate && now - s_tab[i].lastMs < kFreshMs) cand = true;
  portEXIT_CRITICAL(&s_mux);
  return cand ? BleState::CandidateSeen : BleState::Scanning;
}

bool ScooterBLE::getTelemetry(Telemetry& out, uint32_t nowMs) const {
  bool ok = false;
  portENTER_CRITICAL(&s_mux);
  if (s_phase == PhLinked && s_lastFrame != 0 && nowMs - s_lastFrame <= kStaleMs) {
    xbot::fillTelemetry(s_regs, out);
    out.isDemo = false;
    ok = true;
  }
  portEXIT_CRITICAL(&s_mux);
  return ok;
}

BleLinkInfo ScooterBLE::linkInfo() const {
  BleLinkInfo i;
  portENTER_CRITICAL(&s_mux);
  i = s_info;
  xbot::controllerVersion(s_regs, i.version, sizeof(i.version));
  portEXIT_CRITICAL(&s_mux);
  return i;
}
