#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <stdint.h>
#include <Adafruit_ADS1X15.h>
#include <Wire.h>
#include <SensorDesc.h>

class TemperatureSensor {
private:
    static const float TEMP_MIN;
    static const float TEMP_MAX;
    static const float CELSIUS_MAX;
    Adafruit_ADS1115 ads;

public:
    bool init();
    bool read(const SensorDesc &sensor, int32_t &data, int16_t &raw_adc);
};

#endif // TEMPERATURE_H