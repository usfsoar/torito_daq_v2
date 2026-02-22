#include "solenoidreceive.h"

bool SolenoidReceive::init(uint8_t i2c_address) {
    if (i2c_address == 0) return false; // 0 is not a valid 7-bit I2C address here
    i2c_address_ = i2c_address;
    // Do NOT touch/select the I2C mux here — DAQ will select the channel before reads.
    return true;
}

bool SolenoidReceive::read(uint16_t &out_state) {
    if (i2c_address_ == 0) return false;
    
    if (!Wire.requestFrom((int)i2c_address_, (int)2, (int)true)) {
        Serial.print("SolenoidReceive: I2C read failed for addr=0x");
        if (i2c_address_ < 16) Serial.print("0");
        Serial.println(i2c_address_, HEX);
        return false;
    }

    uint8_t hi = (uint8_t)Wire.read();
    uint8_t lo = (uint8_t)Wire.read();

    // MSB-first: hi = high byte, lo = low byte
    out_state = (uint16_t(hi) << 8) | uint16_t(lo);
    cached_state = out_state;

    // success — state cached silently

    return true;
} 

