# Funktionsstatus v0.2.0

## Verificeret (på PC, uden hardware)
* Pure logik (gestus, hold-tracker, touch-kalibrering, sikkerhedsregler, demo-data, "--"-formatering): **2540 checks, 0 fejl** (`tools/run_host_tests.sh`, C++11).
* Alle `.cpp`-filer syntakstjekket (`g++ -fsyntax-only`) mod midlertidige stub-headere for Arduino/TFT_eSPI/NimBLE.
  Stubs afspejler *mine* antagelser om bibliotekernes API – det er **ikke** en rigtig kompilering.

## IKKE verificeret
* Kompilering med PlatformIO, linkning, firmwarestørrelse og versionslåste biblioteker (kræver netværk – gøres af GitHub Actions).
* Opstart, display, farver/inversion, touch, kalibrering, lysstyrke, hastighed/flicker på dit board.
* BLE-scanning (NimBLE) og RAM-forbrug under scanning.
* Alt vedr. E-Wheels e2s Urban Max: **ingen forbindelse, ingen protokol, ingen telemetri**.

## Funktioner
| Funktion | Status |
|---|---|
| Dashboard (hastighed, batteri, spænding, effekt, trip/total, mode/fejl) | Implementeret; viser kun demo-data (mærket DEMO) eller `--` |
| Klokkeslæt | Ikke tilgængeligt (ingen RTC/tidskilde) – viser `--:--` |
| BLE | Scanning + læse-only forbindelse til valgt hjul (profil-detektion, notifikationer, læseforespørgsler). **Ikke testet på hardware.** Ingen skrivning |
| Scooter-protokol | XBOT læsning ported fra kilde (afkodning testet mod optaget ramme). Verificeret **ikke** på Urban Max. Skrivning ikke understøttet (`kVerified=false`, `kWriteSupported=false`) |
| 20 km/t | Lokal sikkerhedsskærm + hold-for-at-aktivere; kun demo-simulation; viser aldrig "bekræftet" uden verificeret protokol og kvittering |
| Musik (iPhone) | Ikke implementeret (planlagt AMS); knapper inaktive |
| Beskeder | Ikke implementeret (planlagt ANCS); intet gemmes |
| Ride controls (WALK/ECO/DRIVE/SPORT, lys, zero start, hazard) | Skærm "CTRL" implementeret; virker kun som lokal demo-simulation. Uden demo er knapperne inaktive og intet sendes. Rigtige kommandoer mangler, fordi protokollen er ukendt |
| Indstillinger gemmes | Implementeret (NVS) – ikke testet på hardware |
