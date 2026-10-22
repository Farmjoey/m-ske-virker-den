#pragma once
#include <stddef.h>
#include <stdio.h>
#include <math.h>

// Writes "--" when the value is not valid. Never invents a number.
inline void fmtFixed(char* out, size_t n, bool valid, float v, int decimals) {
  if (!valid) { snprintf(out, n, "--"); return; }
  if (decimals <= 0) snprintf(out, n, "%d", (int)lroundf(v));
  else snprintf(out, n, "%.*f", decimals, (double)v);
}
