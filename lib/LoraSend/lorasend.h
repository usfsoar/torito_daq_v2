#ifndef LORASEND_H
#define LORASEND_H

#include <Arduino.h>
#include <loramodule.h>
#include <ringbuffer.h>

// LoraSend: drain SampleFrame entries from a RingBuffer, serialize the
// frame header (everything except payload[]), and forward to the
// LoRa module via send_data_hexstr().
class LoraSend {
public:
    LoraSend() = default;

    // Attach objects; return true if parameters look valid.
    bool init(LoraModule* lora_mod, RingBuffer* rb, uint8_t dest_address);

    // Pop a single SampleFrame from the ring buffer, serialize header
    // (timestamp_us, seq, valid_mask, status_bits, raw_adc[]), and send.
    // Returns true on success (frame popped and LoRa module accepted it).
    bool send_next();

    // Drain and send all available frames (returns number of frames sent).
    size_t send_all();

    // Helper: serialize header-only portion of SampleFrame into `out`.
    // Returns number of bytes written or 0 if `out_len` too small.
    static size_t serialize_frame_header(const SampleFrame &frame, uint8_t *out, size_t out_len);

private:
    RingBuffer* ring_buffer = nullptr;
    LoraModule* lora_module = nullptr;
    uint8_t dest_address = 0;

    // Backoff timestamp: when a previous send failed, skip attempts until this time
    uint32_t next_allowed_send_ms = 0;

    // Our header will be <= 20 bytes (timestamp(4)+seq(4)+valid(1)+status(1)+solenoid(2)+raw_adc[4]*2=20)
    static constexpr size_t MAX_SERIALIZED_HEADER = 20;
};

#endif // LORASEND_H