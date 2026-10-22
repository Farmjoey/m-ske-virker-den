# NOVA RIDE OS (v0.2.0 – hardwaretest + læse-only XBOT-forbindelse)

Dashboard-firmware til en ESP32 **CYD 2,8"** (resistiv touch) beregnet til E-Wheels e2s Urban Max.
Version 0.2.0 kan forbinde til hjulet og **læse** telemetri (XBOT-protokollen, se `docs/PROTOCOL.md`). Den skriver **intet** til hjulet. Protokollen er kun verificeret på et andet køretøj, ikke på din Urban Max.

> Status: kildekoden og de rene logik-tests er kørt/syntakstjekket, men **firmwaren er ikke kompileret med PlatformIO og ikke testet på et rigtigt board**
> (udviklingsmiljøet havde ingen netværksadgang). Første rigtige `.bin` bygges af GitHub Actions – se nedenfor.

## 1. Få en `.bin` (GitHub Actions)

1. Opret et tomt repository på GitHub, og upload hele indholdet af denne mappe (inkl. `.github/`).
2. Åbn fanen **Actions** → workflowet *build-firmware* kører automatisk (eller tryk *Run workflow*).
3. Når det er grønt: åbn kørslen → **Artifacts** → hent de fire pakker `NOVA_RIDE_<profil>`.
   Hver indeholder `NOVA_RIDE_<profil>.factory.bin`, `NOVA_RIDE_<profil>.bin` og `SHA256SUMS.txt`.
4. Hvis workflowet fejler, så send mig loggen fra det røde trin – så retter jeg koden.

Workflowet kører først enhedstests, bygger så alle profiler, kontrollerer at appen passer i partitionen (0x140000 bytes) og beregner SHA-256.

## 2. Vælg profil (CYD-varianter)

Din model (USB-C + micro-USB, "CYD2USB") er ikke verificeret fra min side. Community-rapporter siger, at 2-USB-boards ofte bruger ST7789 og kræver inversion, men nogle bruger ILI9341. Prøv i rækkefølge:

| Profil | Når den passer |
|---|---|
| `cyd_st7789` | første forsøg på 2-USB board |
| `cyd_st7789_swaprb` | som ovenfor, men rød/blå er byttet |
| `cyd_ili9341` | klassisk ILI9341 (ofte 1-USB Rv2) |
| `cyd_ili9341_swaprb` | ILI9341 med byttet rød/blå |

Hvis skærmen er hvid/ødelagt/tom: prøv næste driver. Hvis den kun har forkerte farver: brug INVERT (negativ) eller `_swaprb` (rød ↔ blå).

## 3. Flash fra Windows (USB)

1. Tilslut boardet. Installér evt. CH340-/CP210x-driver, hvis der ikke dukker en COM-port op i Enhedshåndtering.
2. Installér Python (python.org) og kør i PowerShell: `python -m pip install esptool`
3. Flash den samlede fil på **adresse 0x0**:
   ```
   python -m esptool --chip esp32 --port COM5 --baud 460800 write_flash 0x0 NOVA_RIDE_cyd_st7789.factory.bin
   ```
   (Ret `COM5` og filnavn.) Kommer den ikke i flash-tilstand: hold **BOOT**, tryk kort **RESET**, slip BOOT.
4. Alternativ uden Python: Espressif *Flash Download Tool*, vælg ESP32, én fil, adresse `0x0`.
5. Kun appen (`NOVA_RIDE_<profil>.bin`) skal på adresse `0x10000` – og kun hvis bootloader/partitioner allerede er flashet med en tidligere `factory.bin`.

Kontrollér checksum: `Get-FileHash .\NOVA_RIDE_cyd_st7789.factory.bin -Algorithm SHA256` og sammenlign med `SHA256SUMS.txt`.

## 4. Første opstart

1. Splash → **touch-kalibrering** (tryk på 3 sigtekorn). Virker uanset akse-bytte/spejling.
2. **HW-test** (4 sider): farvebjælker, orientering, touch-prøve, godkend. INVERT og FLIP findes på knapperne.
3. **BOOT-knappen** (altid): kort tryk = skift inversion, langt tryk (1 s) = vend skærmen 180°.
4. Efter godkendelse starter dashboardet i **DEMO-tilstand** (gul DEMO-markering, testdata).
   Skift mellem profiler nulstiller godkendelsen, så testen køres igen.

## 5. Brug

* Dashboard: stor hastighed, batteri, spænding, effekt, trip/total, mode/fejl. Ukendt = `--`.
* Swipe **op** → 20 km/t-skærmen (kræver at du *holder* knappen 1,5 s; i demo kun lokal simulation).
* CTRL (eller tryk på MODE-teksten): vælg WALK / ECO / DRIVE / SPORT og slå lys, zero start og havarilys til/fra. **Kun demo** indtil protokollen er kendt.
* MUSIC / MSG / SET (demo til/fra, BLE-scan, invert, flip, lysstyrke, HW-test, kalibrering, diagnostik).
* UI-tekster er ASCII (de indbyggede skrifttyper har ikke æ/ø/å).

## 6. Hvad er verificeret og hvad er ikke

Se `docs/STATUS.md` og `docs/COMPATIBILITY.md`.

## 6b. Forbind til hjulet (læse-only)
1. Luk X-BOT-appen. 2. Settings → BLE ON. 3. Settings → DIAG, tryk på dit hjul (vælges som mål). 4. Når DIAG viser `Link ...`, slå DEMO fra. Trin og kontroltal: `docs/PROTOCOL.md`.

## 7. Næste skridt (kræver dig)

* **Urban Max-protokol:** Brug appen *nRF Connect* (iPhone/Android) til at scanne hjulet, notér navn, services/characteristics og hvilke der sender notifikationer under kørsel. Send mig loggen – ingen kommandoer skrives før formatet er dokumenteret.
* **iPhone musik/beskeder:** AMS/ANCS-lag bygges først, når HW-testen og BLE-scanning er bekræftet på dit board.

## Projektstruktur

`src/` moduler: BoardConfig, DisplayUI, TouchInput, ScooterBLE, ScooterProtocol, PhoneMedia, Notifications, SafetyProfile, SettingsStorage, Diagnostics.
`test/host/` enhedstests (`sh tools/run_host_tests.sh`). `tools/package_release.py` samler `.bin`, `.factory.bin` og SHA-256.
Lokalt: `pip install platformio==6.1.16 && pio run -e cyd_st7789`.
