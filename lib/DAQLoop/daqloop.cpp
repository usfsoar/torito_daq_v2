#include "daqloop.h"

// Global state
static uint32_t seq = 0;
static uint32_t tick = 0;

// External buffers (declared in main)
extern RingBuffer daq_buffer;
extern SolenoidReceive solenoid_receive;

void daq_init() {
    seq = 0;
    tick = 0;
}

void daq_step() {
    SampleFrame frame{};              // zero-initialize entire struct (clears padding/unused fields)
    frame.seq = seq++;
    frame.timestamp_us = micros();
    frame.valid_mask = 0;
    frame.status_bits = 0;

    for (int i = 0; i < SENSOR_COUNT; i++) {
        const SensorDesc &desc = sensor_table[i];
        
        // Check if scheduled this frame
        if ((tick % desc.period_ticks) != 0) {
            continue;
        }

        // Select MUX channel if needed (DAQ owns bus control)
        if (desc.mux_channel != NO_MUX) {
            if (!mux_select(desc.bus_id, desc.mux_channel)) {
                frame.status_bits |= MUX_ERR;
                continue;
            }
        }

        // Read sensor (sensor just reads I2C, doesn't touch MUX)
        int32_t processed_value;
        int16_t raw_adc;
        if (sensor_read_dispatch(desc, processed_value, raw_adc)) {
            frame.payload[desc.id] = processed_value;
            frame.raw_adc[desc.id] = raw_adc;
            frame.valid_mask |= (1 << desc.id);
        } else {
            frame.status_bits |= I2C_ERR;
        }
    }

    // DAQ controls the I2C mux selection for solenoid reads (owned bus control).
    // If the mux selection fails, mark MUX_ERR and keep the last-cached solenoid value.
    {
        uint16_t cur = 0;
        if (SOLENOID_MUX_CHANNEL != 0xFF) {
            if (!mux_select(0, SOLENOID_MUX_CHANNEL)) {
                frame.status_bits |= MUX_ERR;
                frame.solenoid_state = solenoid_receive.get_cached_state();
            } else {
                if (!solenoid_receive.read(cur)) {
                    frame.status_bits |= I2C_ERR;
                }
                frame.solenoid_state = solenoid_receive.get_cached_state();
            }
        } else {
            if (!solenoid_receive.read(cur)) {
                frame.status_bits |= I2C_ERR;
            }
            frame.solenoid_state = solenoid_receive.get_cached_state();
        }
    }

    // Push to DAQ buffer (every frame)
    if (!daq_buffer.push(&frame)) {
        frame.status_bits |= OVERRUN;
    }

    tick++;
}
