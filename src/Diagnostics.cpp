#include "Diagnostics.h"
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace Diag {
static const uint8_t kLines = 8;
static const uint8_t kLen = 48;
static char s_lines[kLines][kLen];
static uint8_t s_head = 0;   // next write slot
static uint8_t s_count = 0;

void log(const char* fmt, ...) {
  char buf[kLen];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  strlcpy(s_lines[s_head], buf, kLen);
  s_head = (uint8_t)((s_head + 1) % kLines);
  if (s_count < kLines) ++s_count;
  Serial.println(buf);
}

void logHex(const char* tag, const uint8_t* data, size_t len) {
  char buf[kLen];
  size_t n = snprintf(buf, sizeof(buf), "%s[%u]:", tag, (unsigned)len);
  for (size_t i = 0; i < len && i < 8 && n + 3 < sizeof(buf); ++i)
    n += snprintf(buf + n, sizeof(buf) - n, "%02X", data[i]);
  log("%s", buf);
}

uint8_t lineCount() { return s_count; }

const char* line(uint8_t idx) {
  if (idx >= s_count) return "";
  uint8_t start = (uint8_t)((s_head + kLines - s_count) % kLines);
  return s_lines[(start + idx) % kLines];
}
}  // namespace Diag
