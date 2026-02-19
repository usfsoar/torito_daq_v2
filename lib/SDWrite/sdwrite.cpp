#include "sdwrite.h"

bool SDWrite::init(RingBuffer* buffer, const char *fname) {
    sd_buffer = buffer;
    if (fname) filename = fname;

    if (!SD.begin(BUILTIN_SDCARD)) {
        Serial.println("SD card initialization failed!");
        return false;
    }
    Serial.println("SD card initialized.");

    // append a session marker so analysis can split sessions later
    File f = SD.open(filename, FILE_WRITE);
    if (f) {
        uint32_t marker = SD_MARKER_START;
        f.write(reinterpret_cast<const uint8_t*>(&marker), sizeof(marker));
        f.close();
    } else {
        Serial.println("SDWrite: warning — could not write session marker");
    }

    // buffer starts empty
    write_buffer_index = 0;
    return true;
}

bool SDWrite::flush_buffer() {
    if (write_buffer_index == 0) return true; // nothing to do

    File dataFile = SD.open(filename, FILE_WRITE);
    if (!dataFile) {
        Serial.println("SDWrite: failed to open file for write");
        return false;
    }

    size_t written = dataFile.write(write_buffer, write_buffer_index);
    dataFile.close();

    if (written != write_buffer_index) {
        Serial.println("SDWrite: write incomplete");
        // reset index to avoid re-writing corrupted/partial contents
        write_buffer_index = 0;
        return false;
    }

    write_buffer_index = 0;
    return true;
}

bool SDWrite::data() {
    if (!sd_buffer) {
        Serial.println("SDWrite: sd_buffer not attached");
        return false;
    }

    // drain frames from ringbuffer into the in-memory byte buffer
    while (sd_buffer->pop(&frame)) {
        const size_t fsize = sizeof(frame);
        if (fsize > WRITE_BUFFER_SIZE) {
            Serial.println("SDWrite: single frame larger than WRITE_BUFFER_SIZE");
            return false;
        }

        // flush to SD if incoming frame won't fit
        if (write_buffer_index + fsize > WRITE_BUFFER_SIZE) {
            if (!flush_buffer()) return false;
        }

        memcpy(&write_buffer[write_buffer_index], &frame, fsize);
        write_buffer_index += fsize;

        // if we filled the buffer exactly, flush immediately
        if (write_buffer_index == WRITE_BUFFER_SIZE) {
            if (!flush_buffer()) return false;
        }
    }

    // don't auto-flush here unless full — caller can call forceFlush() when desired
    return true;
}