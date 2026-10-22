// NOVA RIDE OS - firmware entry point.
#include <Arduino.h>
#include "BoardConfig.h"
#include "Diagnostics.h"
#include "SettingsStorage.h"
#include "TouchInput.h"
#include "ScooterBLE.h"
#include "PhoneMedia.h"
#include "Notifications.h"
#include "SafetyProfile.h"
#include "DemoTelemetry.h"
#include "ScooterControl.h"
#include "DisplayUI.h"

static SettingsStorage g_store;
static TouchInput g_touch;
static BootButton g_boot;
static ScooterBLE g_ble;
static PhoneMedia g_phone;
static Notifications g_notes;
static SafetyProfile g_safety;
static ScooterControl g_ctrl;
static DisplayUI g_ui;

void setup() {
  Serial.begin(115200);
  Diag::log("NOVA RIDE OS v%s [%s]", NOVA_FW_VERSION, NOVA_PROFILE_NAME);
  g_store.begin();
  g_touch.begin();
  g_boot.begin();
  g_ble.begin();
  {
    BleTarget bt;
    strlcpy(bt.addr, g_store.get().targetAddr, sizeof(bt.addr));
    strlcpy(bt.name, g_store.get().targetName, sizeof(bt.name));
    bt.type = g_store.get().targetType;
    g_ble.setTarget(bt);
  }
  g_phone.begin();
  g_notes.begin();

  UiContext ctx;
  ctx.store = &g_store;
  ctx.ble = &g_ble;
  ctx.phone = &g_phone;
  ctx.notes = &g_notes;
  ctx.safety = &g_safety;
  ctx.ctrl = &g_ctrl;
  g_ui.begin(ctx);

  if (g_store.get().bleScan) g_ble.setEnabled(true);
}

void loop() {
  const uint32_t now = millis();
  const TouchState ts = g_touch.poll();
  const int b = g_boot.poll(now);
  if (b) g_ui.onBootButton(b == 2);

  g_ble.loop(now);
  g_phone.loop(now);
  g_notes.loop(now);

  // Telemetry source: labelled demo data, or "no data" (all invalid). Never invented.
  Telemetry t;
  const Settings& s = g_store.get();
  if (s.demo) {
    int cap = demoModeCapKmh(g_ctrl.mode());  // demo-only caps per mode
    if (g_safety.demoArmed() && kQuickLimitKmh < cap) cap = kQuickLimitKmh;
    t = demoSample(now, cap);
    char f[4]; int n = 0;
    if (g_ctrl.light()) f[n++] = 'L';
    if (g_ctrl.zeroStart()) f[n++] = 'Z';
    if (g_ctrl.hazard()) f[n++] = 'H';
    f[n] = 0;
    snprintf(t.mode, sizeof(t.mode), "%s%s%s", rideModeName(g_ctrl.mode()), n ? " " : "", f);
  }
  if (!s.demo) {
    g_safety.clearDemo();
    g_ble.getTelemetry(t, now);  // live data only if linked and fresh; otherwise all "--"
  }

  g_ui.update(t, ts, now);
  delay(4);  // yields to FreeRTOS idle task (watchdog-safe)
}
