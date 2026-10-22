#pragma once
// iPhone music control. NOT IMPLEMENTED in v0.1.
// Planned (unverified) route without a companion app: ESP32 as BLE peripheral using
// Apple's Media Service (AMS) for title/artist/transport, plus pairing/bonding.
#include <stdint.h>

enum class PhoneLink : uint8_t { NotImplemented, Disconnected, Connected };

struct MediaInfo {
  PhoneLink link;
  bool trackValid;
  char title[32];
  char artist[32];
  bool playing;
  MediaInfo() : link(PhoneLink::NotImplemented), trackValid(false), playing(false) {
    title[0] = 0; artist[0] = 0;
  }
};

class PhoneMedia {
 public:
  void begin() {}
  void loop(uint32_t) {}
  const MediaInfo& info() const { return info_; }
  bool available() const { return false; }
  bool playPause() { return false; }
  bool next() { return false; }
  bool prev() { return false; }
 private:
  MediaInfo info_;
};
