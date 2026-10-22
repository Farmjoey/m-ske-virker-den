#pragma once
#include <stddef.h>
#include <stdint.h>

namespace Diag {
void log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
// Logs unknown payloads as hex (bounded). Used for unknown BLE data - never answered.
void logHex(const char* tag, const uint8_t* data, size_t len);
uint8_t lineCount();
const char* line(uint8_t idx);  // 0 = oldest
}  // namespace Diag
