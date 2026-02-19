#ifndef SENSORCONFIG_H
#define SENSORCONFIG_H

#include <SensorDesc.h>

#define SENSOR_COUNT 4

// Enable printing of pressure data to Serial from `main` (0 = off, 1 = on)
#ifndef ENABLE_PRESSURE_SERIAL
#define ENABLE_PRESSURE_SERIAL 0
#endif

// Sensor table - configure your hardware here
// NOTE: sensorType must match the dispatcher (LOWPRESSURE or HIGHPRESSURE).
const SensorDesc sensor_table[SENSOR_COUNT] = {
    // id, type, bus_id, mux_channel, i2c_address, adc_channel, period_ticks
    {
        .id = 0,
        .sensorType = SensorDesc::type::LOWPRESSURE,
        .bus_id = 0,
<<<<<<< DAQFeb19th2026
        .mux_channel = 7,           // PCA9548A channel 7 (SD7/SC7)
=======
        .mux_channel = 0,           // TCA9548A channel 0 (SD0/SC0)
>>>>>>> main
        .i2c_address = 0x48,        // ADS1115 address
        .adc_channel = 0,           // ADS1115 input A0
        .period_ticks = 1           // Read every frame
    },
    {
        .id = 1,
        .sensorType = SensorDesc::type::LOWPRESSURE,
        .bus_id = 0,
        .mux_channel = 7,
        .i2c_address = 0x48,
        .adc_channel = 1,           // ADS1115 input A1
        .period_ticks = 1
    },
    {
        .id = 2,
        .sensorType = SensorDesc::type::LOWPRESSURE,
        .bus_id = 0,
        .mux_channel = 7,
        .i2c_address = 0x48,
        .adc_channel = 2,           // ADS1115 input A2
        .period_ticks = 1
    },
    {
        .id = 3,
        .sensorType = SensorDesc::type::HIGHPRESSURE,
        .bus_id = 0,
        .mux_channel = 7,
        .i2c_address = 0x48,
        .adc_channel = 3,           // ADS1115 input A3
        .period_ticks = 1
    }
};

#endif // SENSORCONFIG_H