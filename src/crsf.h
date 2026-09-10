#pragma once

#include <Arduino.h>
#include "config.h"

// CRSF Frame Header constants
#define CRSF_SYNC_BYTE_FC           0xC8
#define CRSF_SYNC_BYTE_RADIO        0xEE
#define CRSF_FRAMETYPE_RC_CHANNELS  0x16
#define CRSF_MAX_FRAME_SIZE         64
#define CRSF_RC_PAYLOAD_SIZE        22

class CRSFReceiver {
public:
    CRSFReceiver();

    // Initialize Serial2 with baud rate and custom RX/TX pins
    void begin(HardwareSerial &serial = Serial2, uint32_t baud = CRSF_BAUDRATE, int rxPin = PIN_CRSF_RX, int txPin = PIN_CRSF_TX);

    // Call frequently in main loop (non-blocking)
    void update();

    // Link state & health
    bool isConnected() const;
    bool isFailsafe() const;
    uint32_t getFrameAgeMs() const;
    uint32_t getValidFrameCount() const { return _validFrameCount; }
    uint32_t getCrcErrorCount() const { return _crcErrorCount; }

    // Channel accessors (0 to 15)
    uint16_t getRawChannel(uint8_t index) const;
    
    // Normalized value: -1.0 (min) to +1.0 (max), centered at 0.0
    float getNormalized(uint8_t index, bool inverted = false, int deadband = 30) const;

    // Get 0-100% value
    float getPercent(uint8_t index) const;

    // Direct access to all 16 raw channels
    const uint16_t* getAllChannels() const { return _channels; }

private:
    HardwareSerial *_serial;
    uint16_t _channels[CRSF_NUM_CHANNELS];
    uint32_t _lastValidFrameMs;
    uint32_t _validFrameCount;
    uint32_t _crcErrorCount;

    // Frame parser state machine
    enum ParserState {
        WAIT_SYNC,
        WAIT_LENGTH,
        WAIT_PAYLOAD
    };

    ParserState _state;
    uint8_t _frameBuffer[CRSF_MAX_FRAME_SIZE];
    uint8_t _frameLength;
    uint8_t _bufferIndex;

    void processByte(uint8_t b);
    void handleFrame();
    void unpackChannels(const uint8_t *payload);
    static uint8_t crc8_d5(const uint8_t *data, uint8_t length);
};

extern CRSFReceiver crsf;
