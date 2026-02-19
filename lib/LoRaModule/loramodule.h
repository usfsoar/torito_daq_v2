#ifndef LORA_MODULE_H
#define LORA_MODULE_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "lora_config.h"

class LoraModule {
public:
    LoraModule(uint8_t address);
    bool begin();
    bool configure(uint8_t address, unsigned long band, uint8_t network_id);
    String send_at_command(const char* command, unsigned long timeout = 1000);
    // send_data_hexstr: send payload as hexadecimal string (existing API)
    bool send_data_hexstr(uint8_t dest_address, String hex_data);
    // Overload: send up to 20 bytes from a byte buffer (will be hex-encoded internally)
    bool send_data_hexstr(uint8_t dest_address, const uint8_t* data, size_t length);
    bool receive_data_hexstr(String& hex_data);
    bool set_parameter(uint8_t sf, uint8_t bw, uint8_t cr, uint8_t preamble);

    // Quick online status (set when AT replies contain "OK").
    bool is_online() const { return _online; }

private:
    HardwareSerial& lora_serial = Serial4;
    uint8_t _rx_pin;
    uint8_t _tx_pin;
    uint8_t _address;

    // True when we have recently observed OK replies from the module
    bool _online = false;
};

#endif
