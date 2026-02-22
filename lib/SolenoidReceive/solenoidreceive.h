#ifndef SOLENOIDRECEIVE_H
#define SOLENOIDRECEIVE_H

#include <Arduino.h>
#include <Wire.h>

// SolenoidReceive: read a 2-byte status payload from another MCU over I2C.
// - Uses `Wire.requestFrom()` to fetch two bytes.
// - Returns a 16-bit bitmask (MSB-first: buf[0] = high byte).
class SolenoidReceive {
public:
    SolenoidReceive() = default;

    // Initialize with the remote MCU I2C address. Does NOT touch the I2C mux;
    // DAQ/owner must select the correct mux channel before calling `read()`.
    bool init(uint8_t i2c_address);

    // Read 2 bytes from the remote MCU into `out_state` (MSB-first).
    // Returns true on success; false if I2C read failed or not initialized.
    bool read(uint16_t &out_state);

    // Return the last-known solenoid state (updated on successful reads).
    uint16_t get_cached_state() const noexcept { return cached_state; }

private:
    uint8_t i2c_address_ = 0;
    uint16_t cached_state = 0;   // last successful read
};

#endif // SOLENOIDRECEIVE_H
