#include "loramodule.h"

LoraModule::LoraModule(uint8_t address) 
    : _address(address) {}

bool LoraModule::begin() {
    lora_serial.begin(LORA_BAUD);
    delay(1000);
    Serial.println("Initializing LoRa Module...");
    if (send_at_command("AT").indexOf("OK") != -1) {
        Serial.println("LoRa module responding");
        return true;
    } else {
        Serial.println("WARNING: LoRa module not responding!");
        return false;
    }
}

bool LoraModule::configure(uint8_t address, unsigned long band, uint8_t network_id) {
    char cmd[50];
    
    // Set address
    sprintf(cmd, "AT+ADDRESS=%d", address);
    send_at_command(cmd);
    delay(100);
    
    // Set band
    sprintf(cmd, "AT+BAND=%lu", band);
    send_at_command(cmd);
    delay(100);
    
    // Set network ID
    sprintf(cmd, "AT+NETWORKID=%d", network_id);
    send_at_command(cmd);
    delay(100);

    // Apply default radio parameters (SF, BW, CR, preamble)
    if (set_parameter(LORA_PARAMETER_SF, LORA_PARAMETER_BW, LORA_PARAMETER_CR, LORA_PARAMETER_PREAMBLE)) {
        Serial.print("LoRa parameters set: ");
        Serial.println(LORA_PARAMETER_DEFAULT_STR);
    } else {
        Serial.println("Warning: failed to set LoRa parameters");
    }

    Serial.println("LoRa configured: Address=" + String(address) + 
                   ", Band=" + String(band) + 
                   ", NetworkID=" + String(network_id));
    return true;
}

bool LoraModule::set_parameter(uint8_t sf, uint8_t bw, uint8_t cr, uint8_t preamble) {
    char cmd[64];
    sprintf(cmd, AT_SET_PARAMETER_FMT, sf, bw, cr, preamble);
    String resp = send_at_command(cmd, AT_COMMAND_TIMEOUT);
    delay(LORA_CONFIG_DELAY);
    return resp.indexOf("OK") != -1;
}

String LoraModule::send_at_command(const char* command, unsigned long timeout) {
    String result = "";
    while (lora_serial.available()) {
        lora_serial.read();
    }
    lora_serial.println(command);
    unsigned long start_time = millis();
    unsigned long last_char_time = millis();
    while (millis() - start_time < timeout) {
        if (lora_serial.available()) {
            char c = lora_serial.read();
            result += c;
            last_char_time = millis();
            if ((result.indexOf("OK") != -1 || result.indexOf("ERROR") != -1) && 
                millis() - last_char_time > 50) {
                break;
            }
        }
    }

    // Update online flag from the response so callers can quickly skip future attempts
    if (result.indexOf("OK") != -1) {
        _online = true;
    } else if (result.length() == 0 || result.indexOf("ERROR") != -1) {
        _online = false;
    }

    Serial.print("Response: ");
    Serial.println(result);
    return result;
}

// existing API: send a hex string payload
bool LoraModule::send_data_hexstr(uint8_t dest_address, String hex_data) {
    // If module previously proved unreachable, skip immediately to avoid long blocking
    if (!_online) return false;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+SEND=%d,%d,%s", dest_address, (int)(hex_data.length() / 2), hex_data.c_str());
    // use short timeout to avoid long AT delays on failure
    String response = send_at_command(cmd, 200);
    return response.indexOf("OK") != -1;
}

// new overload: accept raw bytes (up to 20) and convert to hex internally
bool LoraModule::send_data_hexstr(uint8_t dest_address, const uint8_t* data, size_t length) {
    // enforce the 20-byte maximum you requested
    if (data == nullptr || length == 0 || length > 20) {
        return false;
    }

    // AT+SEND requires a hex string (two ASCII chars per byte)
    // compute required buffer: prefix + 2*length + null
    char cmd[128];
    int prefix_len = snprintf(cmd, sizeof(cmd), "AT+SEND=%d,%d,", dest_address, (int)length);
    if (prefix_len < 0 || prefix_len >= (int)sizeof(cmd)) return false;

    const char hex[] = "0123456789ABCDEF";
    int pos = prefix_len;
    for (size_t i = 0; i < length; ++i) {
        if (pos + 2 >= (int)sizeof(cmd)) return false; // safety
        uint8_t b = data[i];
        cmd[pos++] = hex[(b >> 4) & 0x0F];
        cmd[pos++] = hex[b & 0x0F];
    }
    cmd[pos] = '\0';

    // If module is known-unreachable, skip to avoid waiting
    if (!_online) return false;
    String response = send_at_command(cmd, 200);
    return response.indexOf("OK") != -1;
}

bool LoraModule::receive_data_hexstr(String& hex_data) {
    if (!lora_serial.available()) {
        return false;
    }
    String incoming_string = "";
    // Read with timeout
    unsigned long start_time = millis();
    unsigned long last_char_time = millis();
    while (millis() - start_time < 1000) {
        if (lora_serial.available()) {
            char c = lora_serial.read();
            if (c == '\n') break;
            incoming_string += c;
            last_char_time = millis();
        } else if (millis() - last_char_time > 50) {
            // No data for 50ms, likely complete
            break;
        }
    }
    // Parse format: +RCV=<address>,<length>,<data>,<RSSI>,<SNR>
    int first_comma = incoming_string.indexOf(',');
    int second_comma = incoming_string.indexOf(',', first_comma + 1);
    
    if (first_comma > 0 && second_comma > 0) {
        hex_data = incoming_string.substring(second_comma + 1);
        // Remove RSSI and SNR if present
        int third_comma = hex_data.indexOf(',');
        if (third_comma > 0) {
            hex_data = hex_data.substring(0, third_comma);
        }
        hex_data.trim();
        return true;
    }
    
    return false;
}
