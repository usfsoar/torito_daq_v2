#include "sensordispatcher.h"

// Sesnor class instances
LowPressureSensor g_low_pressure_sensor;
HighPressureSensor g_high_pressure_sensor;

bool sensor_dispatcher_init() {
    if (!g_low_pressure_sensor.init()) {
        Serial.println("ERROR: ADS1115 (low pressure sensor) init failed!");
        return false;
    }
    
    if (!g_high_pressure_sensor.init()) {
        Serial.println("ERROR: ADS1115 (high pressure sensor) init failed!");
        return false;
    }
    
    return true;
}

bool sensor_read_dispatch(const SensorDesc &sensor, int32_t &data, int16_t &raw_adc) {
    switch (sensor.sensorType) {
        case SensorDesc::type::LOWPRESSURE:
            return g_low_pressure_sensor.read(sensor, data, raw_adc);
        case SensorDesc::type::HIGHPRESSURE:
            return g_high_pressure_sensor.read(sensor, data, raw_adc);
        default:
            return false;
    }
}