#include "crsf.h"

CRSFReceiver crsf;

// CRSF CRC8 lookup table (polynomial 0xD5)
static const uint8_t crsf_crc8_table[256] = {
    0x00, 0xD5, 0x7F, 0xAA, 0xFE, 0x2B, 0x81, 0x54, 0x29, 0xFC, 0x56, 0x83, 0xD7, 0x02, 0xA8, 0x7D,
    0x52, 0x87, 0x2D, 0xF8, 0xAC, 0x79, 0xD3, 0x06, 0x7B, 0xAE, 0x04, 0xD1, 0x85, 0x50, 0xFA, 0x2F,
    0xA4, 0x71, 0xDB, 0x0E, 0x5A, 0x8F, 0x25, 0xF0, 0x8D, 0x58, 0xF2, 0x27, 0x73, 0xA6, 0x0C, 0xD9,
    0xF6, 0x23, 0x89, 0x5C, 0x08, 0xDD, 0x77, 0xA2, 0xDF, 0x0A, 0xA0, 0x75, 0x21, 0xF4, 0x5E, 0x8B,
    0x9D, 0x48, 0xE2, 0x37, 0x63, 0xB6, 0x1C, 0xC9, 0xB4, 0x61, 0xCB, 0x1E, 0x4A, 0x9F, 0x35, 0xE0,
    0xCF, 0x1A, 0xB0, 0x65, 0x31, 0xE4, 0x4E, 0x9B, 0xE6, 0x33, 0x99, 0x4C, 0x18, 0xCD, 0x67, 0xB2,
    0x39, 0xEC, 0x46, 0x93, 0xC7, 0x12, 0xB8, 0x6D, 0x10, 0xC5, 0x6F, 0xBA, 0xEE, 0x3B, 0x91, 0x44,
    0x6B, 0xBE, 0x14, 0xC1, 0x95, 0x40, 0xEA, 0x3F, 0x42, 0x97, 0x3D, 0xE8, 0xBC, 0x69, 0xC3, 0x16,
    0xEF, 0x3A, 0x90, 0x45, 0x11, 0xC4, 0x6E, 0xBB, 0xC6, 0x13, 0xB9, 0x6C, 0x38, 0xED, 0x47, 0x92,
    0xBD, 0x68, 0xC2, 0x17, 0x43, 0x96, 0x3C, 0xE9, 0x94, 0x41, 0xEB, 0x3E, 0x6A, 0xBF, 0x15, 0xC0,
    0x4B, 0x9E, 0x34, 0xE1, 0xB5, 0x60, 0xCA, 0x1F, 0x62, 0xB7, 0x1D, 0xC8, 0x9C, 0x49, 0xE3, 0x36,
    0x19, 0xCC, 0x66, 0xB3, 0xE7, 0x32, 0x98, 0x4D, 0x30, 0xE5, 0x4F, 0x9A, 0xCE, 0x1B, 0xB1, 0x64,
    0x72, 0xA7, 0x0D, 0xD8, 0x8C, 0x59, 0xF3, 0x26, 0x5B, 0x8E, 0x24, 0xF1, 0xA5, 0x70, 0xDA, 0x0F,
    0x20, 0xF5, 0x5F, 0x8A, 0xDE, 0x0B, 0xA1, 0x74, 0x09, 0xDC, 0x76, 0xA3, 0xF7, 0x22, 0x88, 0x5D,
    0xD6, 0x03, 0xA9, 0x7C, 0x28, 0xFD, 0x57, 0x82, 0xFF, 0x2A, 0x80, 0x55, 0x01, 0xD4, 0x7E, 0xAB,
    0x84, 0x51, 0xFB, 0x2E, 0x7A, 0xAF, 0x05, 0xD0, 0xAD, 0x78, 0xD2, 0x07, 0x53, 0x86, 0x2C, 0xF9
};

CRSFReceiver::CRSFReceiver()
    : _serial(nullptr),
      _lastValidFrameMs(0),
      _validFrameCount(0),
      _crcErrorCount(0),
      _state(WAIT_SYNC),
      _frameLength(0),
      _bufferIndex(0) {
    for (int i = 0; i < CRSF_NUM_CHANNELS; i++) {
        _channels[i] = CRSF_CHANNEL_MID;
    }
}

void CRSFReceiver::begin(HardwareSerial &serial, uint32_t baud, int rxPin, int txPin) {
    _serial = &serial;
    _serial->setRxBufferSize(1024); // Large RX buffer to ensure zero dropped bytes at 420kbaud
    _serial->begin(baud, SERIAL_8N1, rxPin, txPin);
    _state = WAIT_SYNC;
    _bufferIndex = 0;
    _lastValidFrameMs = 0;
    _validFrameCount = 0;
    _crcErrorCount = 0;
}

uint8_t CRSFReceiver::crc8_d5(const uint8_t *data, uint8_t length) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < length; i++) {
        crc = crsf_crc8_table[crc ^ data[i]];
    }
    return crc;
}

void CRSFReceiver::update() {
    if (!_serial) return;

    while (_serial->available() > 0) {
        uint8_t b = _serial->read();
        processByte(b);
    }
}

void CRSFReceiver::processByte(uint8_t b) {
    switch (_state) {
        case WAIT_SYNC:
            // CRSF destination address: 0xC8 (Flight Controller), 0xEE (Radio), etc.
            if (b == CRSF_SYNC_BYTE_FC || b == CRSF_SYNC_BYTE_RADIO || b == 0xEA || b == 0xEC) {
                _frameBuffer[0] = b;
                _bufferIndex = 1;
                _state = WAIT_LENGTH;
            }
            break;

        case WAIT_LENGTH:
            _frameLength = b;
            if (_frameLength >= 2 && _frameLength <= (CRSF_MAX_FRAME_SIZE - 2)) {
                _frameBuffer[1] = b;
                _bufferIndex = 2;
                _state = WAIT_PAYLOAD;
            } else {
                _state = WAIT_SYNC;
            }
            break;

        case WAIT_PAYLOAD:
            _frameBuffer[_bufferIndex++] = b;
            if (_bufferIndex >= (_frameLength + 2)) {
                handleFrame();
                _state = WAIT_SYNC;
            } else if (_bufferIndex >= CRSF_MAX_FRAME_SIZE) {
                _state = WAIT_SYNC;
            }
            break;
    }
}

void CRSFReceiver::handleFrame() {
    uint8_t frameType = _frameBuffer[2];
    uint8_t expectedCrc = _frameBuffer[_frameLength + 1];
    
    uint8_t calculatedCrc = crc8_d5(&_frameBuffer[2], _frameLength - 1);

    if (calculatedCrc != expectedCrc) {
        _crcErrorCount++;
        return;
    }

    if (frameType == CRSF_FRAMETYPE_RC_CHANNELS) {
        if ((_frameLength - 2) >= CRSF_RC_PAYLOAD_SIZE) {
            unpackChannels(&_frameBuffer[3]);
            _lastValidFrameMs = millis();
            _validFrameCount++;
        }
    }
}

void CRSFReceiver::unpackChannels(const uint8_t *payload) {
    _channels[0]  = ((payload[0])       | (payload[1] << 8))                      & 0x07FF;
    _channels[1]  = ((payload[1] >> 3)  | (payload[2] << 5))                      & 0x07FF;
    _channels[2]  = ((payload[2] >> 6)  | (payload[3] << 2) | (payload[4] << 10)) & 0x07FF;
    _channels[3]  = ((payload[4] >> 1)  | (payload[5] << 7))                      & 0x07FF;
    _channels[4]  = ((payload[5] >> 4)  | (payload[6] << 4))                      & 0x07FF;
    _channels[5]  = ((payload[6] >> 7)  | (payload[7] << 1) | (payload[8] << 9))  & 0x07FF;
    _channels[6]  = ((payload[8] >> 2)  | (payload[9] << 6))                      & 0x07FF;
    _channels[7]  = ((payload[9] >> 5)  | (payload[10] << 3))                     & 0x07FF;
    _channels[8]  = ((payload[11])      | (payload[12] << 8))                     & 0x07FF;
    _channels[9]  = ((payload[12] >> 3) | (payload[13] << 5))                     & 0x07FF;
    _channels[10] = ((payload[13] >> 6) | (payload[14] << 2) | (payload[15] << 10))& 0x07FF;
    _channels[11] = ((payload[15] >> 1) | (payload[16] << 7))                     & 0x07FF;
    _channels[12] = ((payload[16] >> 4) | (payload[17] << 4))                     & 0x07FF;
    _channels[13] = ((payload[17] >> 7) | (payload[18] << 1) | (payload[19] << 9)) & 0x07FF;
    _channels[14] = ((payload[19] >> 2) | (payload[20] << 6))                     & 0x07FF;
    _channels[15] = ((payload[20] >> 5) | (payload[21] << 3))                     & 0x07FF;
}

bool CRSFReceiver::isConnected() const {
    if (_lastValidFrameMs == 0) return false;
    return (millis() - _lastValidFrameMs) < CRSF_FAILSAFE_MS;
}

bool CRSFReceiver::isFailsafe() const {
    return !isConnected();
}

uint32_t CRSFReceiver::getFrameAgeMs() const {
    if (_lastValidFrameMs == 0) return 999999;
    return millis() - _lastValidFrameMs;
}

uint16_t CRSFReceiver::getRawChannel(uint8_t index) const {
    if (index >= CRSF_NUM_CHANNELS) return CRSF_CHANNEL_MID;
    return _channels[index];
}

float CRSFReceiver::getNormalized(uint8_t index, bool inverted, int deadband) const {
    if (isFailsafe()) return 0.0f;
    uint16_t raw = getRawChannel(index);

    int32_t delta = (int32_t)raw - CRSF_CHANNEL_MID;

    if (abs(delta) <= deadband) {
        return 0.0f;
    }

    float norm = 0.0f;
    if (delta > 0) {
        norm = (float)(delta - deadband) / (float)(CRSF_CHANNEL_MAX - CRSF_CHANNEL_MID - deadband);
    } else {
        norm = (float)(delta + deadband) / (float)(CRSF_CHANNEL_MID - CRSF_CHANNEL_MIN - deadband);
    }

    norm = constrain(norm, -1.0f, 1.0f);
    if (inverted) norm = -norm;
    return norm;
}

float CRSFReceiver::getPercent(uint8_t index) const {
    uint16_t raw = getRawChannel(index);
    float pct = (float)(raw - CRSF_CHANNEL_MIN) / (float)(CRSF_CHANNEL_MAX - CRSF_CHANNEL_MIN) * 100.0f;
    return constrain(pct, 0.0f, 100.0f);
}
