# Hardwarekompatibilitet (kort rapport)

**Intet er verificeret på dit fysiske board.** Det der står nedenfor er fra community-kilder og skal bekræftes af HW-testen.

| Emne | Antagelse i koden | Evidens / status |
|---|---|---|
| MCU | Klassisk ESP32-WROOM-32, 4 MB flash, ingen PSRAM | Typisk for CYD; ikke målt |
| Display-pins | MISO12 MOSI13 SCLK14 CS15 DC2 BL21, RST ikke tilsluttet | Matcher flere community-konfigurationer (TFT_eSPI-opsætninger til ESP32-2432S028) |
| Touch | XPT2046 på egne pins: CLK25 MOSI32 MISO39 CS33 IRQ36, læst via bit-banging | Matcher community-eksempler (XPT2046_Bitbang 32/39/25/33) |
| Displaydriver | ILI9341 (ofte 1-USB) eller ST7789 (ofte "2USB"-varianten) | Community: 2-USB-boards findes i begge varianter, kræver ofte inversion og/eller byttet rød/blå – **derfor 4 profiler** |
| Rv2 vs Rv3 | Ikke skelnet i koden; kun driver/farverækkefølge/inversion/rotation justeres | PCB-revision kan ikke aflæses i software |
| Rotation | Landscape 1 og 3 (320×240) | Sikres af HW-test + FLIP |
| Touch-mapping | 3-punkts kalibrering gemt pr. rotation | Testet som ren logik på PC (alle akse-bytte/spejl-kombinationer) |
| Flash | Standard 4 MB-partitionering, app maks 0x140000 bytes | Størrelse tjekkes i CI |
