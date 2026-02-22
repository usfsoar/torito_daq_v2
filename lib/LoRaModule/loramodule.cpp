#include "loramodule.h"

LoraModule::LoraModule(uint8_t address) 
    : _address(address) {}

bool LoraModule::begin() {
    lora_serial.begin(LORA_BAUD);
    delay(1000);
    Serial.println("Initializing LoRa Module...");
    String result = send_at_command("AT");
    Serial.print("Response: ");
    Serial.println(result);
    if (result.indexOf("OK") != -1) {
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
    // flush any stale data
    while (lora_serial.available()) {
        lora_serial.read();
    }

    // show exact command we are sending for debug
    Serial.print("AT>>> ");
    Serial.println(command);

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
    bool old_online = _online;
    if (result.indexOf("OK") != -1) {
        _online = true;
    } else if (result.indexOf("ERROR") != -1) {
        // explicit error reply means we really can't trust module
        _online = false;
    }
    // do not flip _online on empty response; a timeout alone shouldn't
    // permanently mark the module offline, it may simply be busy.
    if (_online != old_online) {
        Serial.print("Module online status changed: ");
        Serial.println(_online ? "ONLINE" : "OFFLINE");
    }

    if (result.length() == 0) {
        Serial.println("(no response received - timeout)");
    }

    Serial.print("Response: ");
    Serial.println(result);
    return result;
}

// existing API: send a hex string payload
bool LoraModule::send_data_hexstr(uint8_t dest_address, String hex_data) {
    // If module previously proved unreachable, skip immediately to avoid long blocking
    if (!_online) return false;

    // NOTE: some LoRa firmware (eg. the one on our module) expects the length
    // parameter to equal the number of hex digits, not the number of bytes.
    // Historically we passed hex_data.length()/2, but +ERR=5 showed that the
    // module was comparing against hex_data.length() itself.  Use the safe
    // value here so that both firmware variants work.
    int hexChars = (int)hex_data.length();
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+SEND=%d,%d,%s", dest_address, hexChars, hex_data.c_str());
    // use short timeout to avoid long AT delays on failure
    String response = send_at_command(cmd, 200);
    if (response.indexOf("OK") != -1) {
        Serial.println("LoRa send successful");
        return true;
    }
    // Some firmware variants give no response for AT+SEND – the command is
    // treated asynchronously, and an absence of text does *not* mean the
    // packet failed to transmit.  Treat an empty response as success but
    // log a warning so we can differentiate from a real error later.
    if (response.length() == 0) {
        Serial.println("LoRa send: no response, assuming success");
        return true;
    }
    if (response.indexOf("+ERR") != -1) {
        Serial.println("LoRa send failed, dumping debug info:");
        Serial.print("  AT command: "); Serial.println(cmd);
        Serial.print("  hexChars param: "); Serial.println(hexChars);
        Serial.print("  hex length: "); Serial.println(hex_data.length());
        Serial.print("  hex data: "); Serial.println(hex_data);
    }
    return false;
}

// new overload: accept raw bytes (up to 20) and convert to hex internally
bool LoraModule::send_data_hexstr(uint8_t dest_address, const uint8_t* data, size_t length) {
    // enforce the 20-byte maximum you requested
    if (data == nullptr || length == 0 || length > 20) {
        Serial.println("Data length exceeds 20 bytes");
        return false;
    }

    // AT+SEND requires a hex string (two ASCII chars per byte)
    // compute required buffer: prefix + 2*length + null
    char cmd[128];
    // for raw-buffer variant we also supply hex-digit count (length*2)
    int prefix_len = snprintf(cmd, sizeof(cmd), "AT+SEND=%d,%d,", dest_address, (int)(length * 2));
    if (prefix_len < 0 || prefix_len >= (int)sizeof(cmd)) return false;

    const char hex[] = "0123456789ABCDEF";
    int pos = prefix_len;
    for (size_t i = 0; i < length; ++i) {
        if (pos + 2 >= (int)sizeof(cmd)) {
            Serial.println("Command buffer overflow");
            return false; // safety
        }
        uint8_t b = data[i];
        cmd[pos++] = hex[(b >> 4) & 0x0F];
        cmd[pos++] = hex[b & 0x0F];
    }
    cmd[pos] = '\0';

    // If module is known-unreachable, skip to avoid waiting
    
    if (!_online) {
        Serial.println("Module offline, skipping send");
        return false;
    }
    String response = send_at_command(cmd, 200);
    if (response.indexOf("OK") != -1) {
        Serial.println("LoRa send successful");
        return true;
    }
    if (response.length() == 0) {
        Serial.println("LoRa send: no response, assuming success");
        return true;
    }
    if (response.indexOf("+ERR") != -1) {
        int actualHexChars = pos - prefix_len;
        Serial.println("LoRa send failed, dumping debug info:");
        Serial.print("  AT command: "); Serial.println(cmd);
        Serial.print("  hexChars param: "); Serial.println((int)(length * 2));
        Serial.print("  actualHexChars: "); Serial.println(actualHexChars);
    }
    return false;
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
