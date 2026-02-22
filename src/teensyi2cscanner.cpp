#include <Wire.h>

const char* i2cStatusToString(uint8_t s) {
  switch (s) {
    case 0: return "OK";
    case 1: return "DATA_TOO_LONG";
    case 2: return "NACK_ON_ADDR";
    case 3: return "NACK_ON_DATA";
    case 4: return "OTHER_ERROR";
    default: return "UNKNOWN";
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Wire.begin();            // join I2C bus as master (scanner)
  Serial.println("\nI2C Scanner");
}

void loop() {
  byte error, address;
  int count = 0;
  Serial.println("Scanning I2C addresses...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    Serial.print("probe 0x");
    if (address < 16) Serial.print("0");
    Serial.print(address, HEX);
    Serial.print(" -> endTransmission=");
    Serial.print(error);
    Serial.print(" (");
    Serial.print(i2cStatusToString(error));
    Serial.println(")");

    if (error == 0) {
      Serial.print("Found device at 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      if (address == 0x09) {
        Serial.println("  <-- TARGET SLAVE (0x09) detected on main bus");
      }
      count++;
      delay(10);

      // If this is the TCA9548A mux, dump its control byte and probe each channel
      if (address == 0x70) {
        Serial.println("TCA9548A (0x70) detected — dumping mux control + probing channels...");

        // Read control byte (which channels are currently enabled)
        size_t got = Wire.requestFrom((int)0x70, (int)1, (int)true);
        Serial.print("Wire.requestFrom(0x70,1) -> got="); Serial.println(got);
        if (got >= 1) {
          uint8_t ctl = (uint8_t)Wire.read();
          Serial.print("Mux control byte = 0x"); if (ctl < 16) Serial.print("0"); Serial.println(ctl, HEX);
        }

        // Probe all 8 channels and print raw Wire debug
        for (uint8_t ch = 0; ch < 8; ++ch) {
          uint8_t mask = (1 << ch);
          Serial.print("\nSelecting mux channel "); Serial.print(ch);
          Serial.print(" (mask 0x"); if (mask < 16) Serial.print("0"); Serial.print(mask, HEX); Serial.print(") -> ");

          Wire.beginTransmission(0x70);
          Wire.write(mask);
          byte selErr = Wire.endTransmission();
          Serial.print("endTransmission="); Serial.print(selErr);
          Serial.print(" ("); Serial.print(i2cStatusToString(selErr)); Serial.println(")");

          delay(5); // allow switch

          // Re-read control register after select
          got = Wire.requestFrom((int)0x70, (int)1, (int)true);
          Serial.print(" post-select control read -> got="); Serial.print(got);
          if (got >= 1) {
            uint8_t ctl2 = (uint8_t)Wire.read();
            Serial.print(" control=0x"); if (ctl2 < 16) Serial.print("0"); Serial.println(ctl2, HEX);
          } else {
            Serial.println();
          }

          // Downstream scan for this mux channel — print raw Wire endTransmission results
          Serial.println("  Downstream scan (raw Wire endTransmission results):");
          const uint8_t interesting[] = { 0x09, 0x48, 0x49, 0x4A, 0x4B };
          const size_t interestingCount = sizeof(interesting) / sizeof(interesting[0]);
          bool foundInteresting = false;
          for (uint8_t a = 1; a < 127; ++a) {
            Wire.beginTransmission(a);
            byte r = Wire.endTransmission();
            Serial.print("    addr 0x"); if (a < 16) Serial.print("0"); Serial.print(a, HEX);
            Serial.print(" -> "); Serial.print(r);
            Serial.print(" ("); Serial.print(i2cStatusToString(r)); Serial.print(")");
            if (r == 0) {
              Serial.print("  DEVICE at 0x"); if (a < 16) Serial.print("0"); Serial.print(a, HEX);
              // highlight expected/important addresses (ADS + user slave 0x09)
              for (size_t ii = 0; ii < interestingCount; ++ii) {
                if (a == interesting[ii]) {
                  Serial.print("  <-- expected/important (0x"); if (interesting[ii] < 16) Serial.print("0"); Serial.print(interesting[ii], HEX); Serial.print(")");
                  foundInteresting = true;
                  break;
                }
              }
            }
            Serial.println();
            delay(1);
          }
          if (foundInteresting) {
            Serial.print("  -> Important device(s) found on mux channel ");
            Serial.println(ch);
          }
        }

        Serial.println("\nFinished mux channel probe for 0x70\n");
      }
    }
  }

  if (count == 0) Serial.println("No devices found");
  else { Serial.print("Done — devices found: "); Serial.println(count); }

  delay(5000);
}