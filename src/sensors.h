#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include "config.h"

struct SensorData {
    // Environmental readings
    float temperature;      // °C
    float humidity;         // %
    float dewPoint;         // °C (Calculated)
    float heatIndex;        // °C (Calculated)
    uint8_t fogRisk;        // 0=Low, 1=Moderate, 2=High/Critical Fog Condensation
    bool dhtValid;

    // Direct Hazard Flags
    bool flameDetected;     // GPIO33 Active LOW
    bool gasDetected;       // GPIO35 Active LOW

    // Ultrasonic Distance & Proximity
    float distanceCm;       // Distance in cm (2..400)
    bool obstacleWarning;   // True if within stop buffer
    uint8_t proximityZone;  // 0=Clear (>100cm), 1=Caution (40-100cm), 2=Critical (<40cm)
    bool sonarResponding;   // True if ultrasonic echoes are actively being received

    // MPU6050 6-DOF IMU & Attitude
    float accelX;           // m/s^2
    float accelY;           // m/s^2
    float accelZ;           // m/s^2
    float gyroX;            // rad/s
    float gyroY;            // rad/s
    float gyroZ;            // rad/s
    float pitchDeg;         // Pitch angle (degrees)
    float rollDeg;          // Roll angle (degrees)
    float totalTiltDeg;     // Total tilt from horizontal (degrees)
    float gForce;           // G-force magnitude (g)
    bool tiltHazard;        // True if tilt exceeds threshold or inverted
    bool mpuAvailable;      // True if MPU6050 initialized successfully

    // Multi-Sensor Fusion Threat Matrix
    uint8_t threatScore;    // 0 (Nominal) to 100 (Critical Hazard)
    uint8_t threatLevel;    // 0=NOMINAL, 1=ADVISORY, 2=WARNING, 3=CRITICAL
    
    uint32_t lastUpdateMs;
};

class SensorManager {
public:
    SensorManager();

    void begin();
    void update(); // Non-blocking update called in main loop

    const SensorData& getData() const { return _data; }
    SensorData& getData() { return _data; }

    bool isAnyHazardActive() const {
        return _data.flameDetected || _data.gasDetected || _data.tiltHazard;
    }

private:
    SensorData _data;
    DHT _dht;
    Adafruit_MPU6050 _mpu;

    uint32_t _lastDhtPollMs;
    uint32_t _lastSonicTriggerMs;
    uint32_t _lastMpuPollMs;

    void readDigitalSensors();
    void triggerUltrasonicPulse();
    void processUltrasonicEcho();
    void readDHT();
    void readMPU();
    void computeSensorFusion();
};

extern SensorManager sensors;
