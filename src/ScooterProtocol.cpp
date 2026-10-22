#include "ScooterProtocol.h"
#include "Diagnostics.h"

namespace ScooterProtocol {

bool parseNotification(const uint8_t* data, size_t len, Telemetry& out) {
  (void)out;  // never touched: no verified format exists
  Diag::logHex("unk", data, len);
  return false;
}

bool requestSpeedLimit(uint8_t kmh) {
  (void)kmh;  // intentionally no I/O
  return false;
}

bool writeMode(uint8_t mode) { (void)mode; return false; }
bool writeLight(bool on) { (void)on; return false; }
bool writeZeroStart(bool on) { (void)on; return false; }
bool writeHazard(bool on) { (void)on; return false; }

}  // namespace ScooterProtocol
