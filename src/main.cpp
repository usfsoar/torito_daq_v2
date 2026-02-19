// src/main.cpp
#include <Arduino.h>
#include <Wire.h>
#include <hwconfig.h>
#include <daqloop.h>
#include <ringbuffer.h>
#include <sensordispatcher.h>
#include <muxdriver.h>
#include <sdwrite.h>
#include <dispatcherthread.h>
#include <loramodule.h>
#include <lorasend.h>
#include <lora_config.h>
#include <solenoidreceive.h>

// Define the buffers
RingBuffer daq_buffer;
RingBuffer sd_buffer;
RingBuffer lora_buffer;

// SD writer instance (drains `sd_buffer` and performs block writes)
SDWrite sdwriter;

// LoRa module + sender
// NOTE: LoRa uses a hardware UART. On Teensy this project uses Serial4 (RX4/TX4).
// Constructor pins are ignored on Teensy — only the module address is required.
LoraModule lora_module(LORA_SENDER_ADDRESS);
LoraSend lora_sender;
SolenoidReceive solenoid_receive;
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("DAQ System Starting...");
    
    // Initialize I2C (Teensy 4.1 uses default pins SDA=18, SCL=19)
    Wire.begin();
    
    // Initialize mux hardware
    while (!mux_init()) {
        Serial.println("ERROR: Mux init failed!");
        delay(1000);
    }
    
    // Select mux channel for sensor initialization
    // (All sensors are on channel 7 per sensorconfig.h)
    while (!mux_select(0, 7)) {
        Serial.println("ERROR: Mux channel select failed!");
        delay(1000);
    }
    delay(5);  // Allow mux to stabilize
    
    // Initialize sensors
    while (!sensor_dispatcher_init()) {
        Serial.println("ERROR: Sensor init failed!");
        delay(1000);
    }
    
    daq_init();

    // Initialize SD writer (attach the sd_buffer the dispatcher pushes into)
    if (!sdwriter.init(&sd_buffer, "data.bin")) {
        Serial.println("ERROR: SD writer init failed — continuing without SD logging");
    }

    // Initialize LoRa module and sender
    if (!lora_module.begin()) {
        Serial.println("WARNING: LoRa module did not respond during init");
    } else {
        lora_module.configure(LORA_SENDER_ADDRESS, LORA_BAND, LORA_NETWORK_ID);
        lora_sender.init(&lora_module, &lora_buffer, LORA_RECEIVER_ADDRESS);
    }
    // Initialize SolenoidReceive instance
    if (!solenoid_receive.init(SOLENOID_I2C_ADDR)) {
        Serial.println("ERROR: SolenoidReceive init failed!");
    }

    Serial.println("DAQ Ready!");

#if ENABLE_PRESSURE_SERIAL
    // Serial header for pressure columns (P0..)
    for (int i = 0; i < SENSOR_COUNT; ++i) {
        Serial.print("P"); Serial.print(i);
        if (i < SENSOR_COUNT - 1) Serial.print('\t'); else Serial.println();
    }
#endif
}


void loop() {
    static uint32_t next_daq = 0;
    
    // Run DAQ at 20Hz (50ms)
    if (millis() >= next_daq) {
        daq_step();

#if ENABLE_PRESSURE_SERIAL
        // --- Serial pressure sensor read (one line per DAQ frame) ---
        for (int i = 0; i < SENSOR_COUNT; ++i) {
            const SensorDesc &desc = sensor_table[i];
            // Ensure MUX is selected for this sensor (DAQ normally does this)
            if (desc.mux_channel != NO_MUX) {
                if (!mux_select(desc.bus_id, desc.mux_channel)) {
                    Serial.print("P"); Serial.print(desc.id); Serial.print(": MUX_ERR");
                    if (i < SENSOR_COUNT - 1) Serial.print('\t'); else Serial.println();
                    continue;
                }
                delay(1); // small settle time
            }

            int32_t processed = 0;
            int16_t raw_adc = 0;
            if (sensor_read_dispatch(desc, processed, raw_adc)) {
                float psi = processed / 100.0f; // stored as psi*100
                Serial.print(psi, 2);
            } else {
                Serial.print("ERR");
            }

            if (i < SENSOR_COUNT - 1) Serial.print('\t'); else Serial.println();
        }
        // --- end serial read ---
#endif

        next_daq += 50;
    }
    if (!dispatcher_thread_step()) {
        Serial.println("ERROR: Dispatcher thread step failed! Overflow detetcted!");
    }

    // Drive SD writer (drains `sd_buffer` and writes in blocks).
    // NOTE: `sd_buffer` is now owned/consumed by the SD writer —
    // don't pop it elsewhere (use a debug buffer or peek API for prints).
    sdwriter.data();

    // Attempt to send one pending LoRa frame each loop (if available)
    lora_sender.send_next();
}