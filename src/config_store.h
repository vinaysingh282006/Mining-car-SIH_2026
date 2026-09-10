#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "config.h"

struct RoverConfig {
    // Channel assignments (0 to 15)
    uint8_t throttleChannel;
    uint8_t yawChannel;
    uint8_t tiltChannel;
    uint8_t auxChannel;

    // Channel polarity / inversions
    bool invertThrottle;
    bool invertYaw;
    bool invertTilt;

    // Operational mode
    uint8_t auxMode; // 0=Arm/Disarm, 1=Headlamp/Aux, 2=Return Idle, 3=Auto-Arm (Always Armed)

    // Control parameters & safety thresholds
    uint16_t deadband;
    float tiltLimitDeg;
    float obstacleStopDistanceCm;
    bool enableObstacleBrake; // Safety brake toggle
    uint8_t maxDriveSpeed; // 0-255

    // WiFi Configuration
    char apSSID[33];
    char apPassword[65];
    char staSSID[33];
    char staPassword[65];
};

class ConfigStore {
public:
    ConfigStore();

    void begin();
    void load();
    void save();
    void resetDefaults();

    RoverConfig& get() { return _config; }
    const RoverConfig& get() const { return _config; }

    // Serialization helpers
    void serializeMapping(JsonObject &root) const;
    bool deserializeMapping(const JsonObjectConst &root);

    void serializeWiFi(JsonObject &root) const;
    bool deserializeWiFi(const JsonObjectConst &root);

    void serializeFull(JsonObject &root) const;

private:
    Preferences _prefs;
    RoverConfig _config;
};

extern ConfigStore configStore;
