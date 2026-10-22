# XBOT-protokol i NOVA RIDE OS (v0.2.0, kun læsning)

Kilde: open-source komponenten *esphome-xbot* (`components/xbot/*` og dens tests).
Den er **verificeret på ét 36 V-køretøj**. Din E-Wheels e2s Urban Max (48 V, controller a5.9.6)
er ikke verificeret. Koden i `src/XbotProtocol.h` er en port af den komponent.

## Automatisk protokolfinding (v0.2.0)
Når du vælger hjulet på DIAG, tester firmwaren selv: hver dokumenteret profil, som hjulet har (både write og notify), og for hver profil to dokumenterede nøgler til variant 3 (nøglen fra navnet først, derefter den anden: `0x34` eller klartekst). Under testen sendes kun læseforespørgsler. En kombination accepteres **kun**, når hjulet svarer med en ramme med `55 AA`-header og gyldig checksum. Findes der ingen, står årsagen på DIAG (fx `no reply to read requests`), og den prøver igen. Hjul, hvis navn peger på en anden variant (andre rammeformater), afvises uden at sende noget. Resultatet gemmes ikke; testen kører ved hver forbindelse (få sekunder).

## Fra kilden (dokumenteret, ikke verificeret på dit hjul)
| Del | Indhold |
|---|---|
| Transportprofiler | 5 (nus, ae00, ffe0, fff0-f3f7, fff0-f2f1). Firmwaren vælger den første, hvor hjulet *har både* write- og notify-karakteristik |
| Ramme | `55 AA <len+2> <dest> <cmd> <reg> <payload> <~sum LE>`; checksum fra længde-byten |
| Obfuskering | Hele rammen XOR'es med nøgle fra enhedens navn (variant 3: `0x34`, nogle navne i klartekst) |
| Læseforespørgsler | 20 blokke (`kPollTable`), payload = antal bytes |
| Registre (læses) | hastighed 0x27, batteri 0x22, spænding 0x47, strøm 0x48, trip 0x2F, total 0x29/0x2A, fejl 0x1B, advarsel 0x1C, controller-version 0x1E+0x1D, køretilstand 0x7E |
| Test | Én optaget svarramme fra kilden afkodes korrekt i enhedstesten |

## Antagelser (ikke bekræftet for dit hjul)
* At dit hjul bruger variant 3. Navnet "Urban"/"e2s" står ikke i navnetabellen, så fallback er variant 3 med nøgle `0x34`. Firmwaren sender kun læseforespørgsler; en forkert nøgle gør bare, at hjulet ikke svarer.
* At registerkortet og skaleringerne gælder for en 48 V-controller.
* Køretilstand: kilden mapper 0..3 til Sport/Drive/Eco/Walking. Det er kun bekræftet på ét køretøj, så dashboardet viser kun råindekset (`MODE #n`).

## Ikke implementeret med vilje
* **Ingen skrivning:** lys, zero start, køretilstand, hastighedsgrænser og lås sendes aldrig. CTRL-skærmen er demo-only; uden demo står der "READ-ONLY".
* **Havarilys:** findes ikke i kilden.
* 20 km/t kan ikke bekræftes (`kVerified=false`).

## Sådan verificerer du på hjulet (DIAG-skærmen)
1. Luk X-BOT-appen helt (hjulet tager kun én forbindelse).
2. Settings → BLE ON. DIAG viser enhederne; tryk på dit hjul for at vælge det.
3. DIAG skal vise `Link <profil> key <XX> var 3`. Skriv profilen og nøglen ned.
4. `ntf` og `frm` tæller notifikationer og afkodede rammer. `ver` er controller-versionen (hex).
   Sammenlign med X-BOT: `a5.9.6 (06051153)`.
5. Slå DEMO fra i Settings. Dashboardet viser nu LIVE-data. Kontrollér mod hjulet:
   spænding ≈ 48 V-pakken, batteri %, total km, trip km, og at hastigheden følger virkeligheden.
6. Står der en gul linje (fx `wire key 34, using 00` eller `unknown framing`), så send den til mig.
Først når tallene passer, kan skrivning overvejes.
