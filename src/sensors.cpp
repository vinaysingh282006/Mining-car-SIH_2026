#include "sensors.h"
#include "config_store.h"

SensorManager sensors;

// Non-blocking interrupt variables for HC-SR04 ultrasonic echo timing
static volatile uint32_t echoStartUs = 0;
static volatile uint32_t echoDurationUs = 0;
static volatile bool newEchoReady = false;
static volatile uint32_t lastEchoInterruptMs = 0;

static void IRAM_ATTR echoInterruptHandler() {
    if (digitalRead(PIN_ECHO) == HIGH) {
        echoStartUs = micros();
    } else {
        uint32_t now = micros();
        if (echoStartUs > 0 && now >= echoStartUs) {
            echoDurationUs = now - echoStartUs;
            newEchoReady = true;
            lastEchoInterruptMs = millis();
        }
    }
}

SensorManager::SensorManager()
    : _dht(PIN_DHT, DHT11),
      _lastDhtPollMs(0),
      _lastSonicTriggerMs(0),
      _lastMpuPollMs(0) {
    _data.temperature = 24.0f;
    _data.humidity = 55.0f;
    _data.dewPoint = 14.5f;
    _data.heatIndex = 24.0f;
    _data.fogRisk = 0;
    _data.dhtValid = false;

    _data.flameDetected = false;
    _data.gasDetected = false;

    _data.distanceCm = 400.0f;
    _data.obstacleWarning = false;
    _data.proximityZone = 0;
    _data.sonarResponding = false;

    _data.accelX = 0;
    _data.accelY = 0;
    _data.accelZ = 9.81f;
    _data.gyroX = 0;
    _data.gyroY = 0;
    _data.gyroZ = 0;
    _data.pitchDeg = 0.0f;
    _data.rollDeg = 0.0f;
    _data.totalTiltDeg = 0.0f;
    _data.gForce = 1.0f;
    _data.tiltHazard = false;
    _data.mpuAvailable = false;

    _data.threatScore = 0;
    _data.threatLevel = 0;
    _data.lastUpdateMs = 0;
}

void SensorManager::begin() {
    // Configure Digital Sensor Pins
    pinMode(PIN_FLAME, INPUT_PULLUP);
    pinMode(PIN_GAS, INPUT); // GPIO35 is input-only on ESP32

    // Configure Ultrasonic Pins with Non-Blocking Hardware Interrupt
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    digitalWrite(PIN_TRIG, LOW);
    attachInterrupt(digitalPinToInterrupt(PIN_ECHO), echoInterruptHandler, CHANGE);

    // Initialize DHT11
    _dht.begin();

    // Initialize I2C for MPU6050
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);

    if (_mpu.begin(0x68, &Wire)) {
        _data.mpuAvailable = true;
        _mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
        _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        Serial.println("[SENSORS] MPU6050 6-DOF IMU initialized.");
    } else {
        _data.mpuAvailable = false;
        Serial.println("[SENSORS] MPU6050 not detected. Using orientation fallback.");
    }
}

void SensorManager::readDigitalSensors() {
    // IR Flame Sensor: Active LOW (LOW indicates fire detected)
    _data.flameDetected = (digitalRead(PIN_FLAME) == LOW);

    // MQ135 Gas Sensor: Active LOW (LOW indicates gas threshold exceeded)
    _data.gasDetected = (digitalRead(PIN_GAS) == LOW);
}

void SensorManager::triggerUltrasonicPulse() {
    // Instantaneous 10us trigger pulse (non-blocking)
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);
}

void SensorManager::processUltrasonicEcho() {
    uint32_t now = millis();

    if (newEchoReady) {
        newEchoReady = false;
        float cm = (float)echoDurationUs * 0.01715f;
        if (cm >= 2.0f && cm <= 400.0f) {
            _data.distanceCm = cm;
            _data.sonarResponding = true;
        }
    } else if (now - lastEchoInterruptMs > 300) {
        // No echo received in >300ms (open space or sensor unpowered)
        _data.distanceCm = 400.0f;
        _data.sonarResponding = false;
    }

    float stopDist = configStore.get().obstacleStopDistanceCm;
    
    // Only flag obstacle if sonar is actively responding and object is within stop buffer
    if (_data.sonarResponding && _data.distanceCm >= 3.0f && _data.distanceCm <= stopDist) {
        _data.obstacleWarning = true;
    } else {
        _data.obstacleWarning = false;
    }

    if (_data.sonarResponding && _data.distanceCm <= 35.0f) {
        _data.proximityZone = 2; // Critical
    } else if (_data.sonarResponding && _data.distanceCm <= 90.0f) {
        _data.proximityZone = 1; // Caution
    } else {
        _data.proximityZone = 0; // Clear
    }
}

void SensorManager::readDHT() {
    float t = _dht.readTemperature();
    float h = _dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
        _data.temperature = t;
        _data.humidity = h;
        _data.dhtValid = true;

        // Dew Point approximation: Td = T - ((100 - H) / 5)
        _data.dewPoint = t - ((100.0f - h) / 5.0f);

        // Heat Index calculation (simplified Rothfusz)
        _data.heatIndex = -8.78469475556 + 1.61139411 * t + 2.33854883889 * h
                         - 0.14611605 * t * h - 0.012308094 * t * t
                         - 0.0164248277778 * h * h + 0.002211732 * t * t * h
                         + 0.00072546 * t * h * h - 0.000003582 * t * t * h * h;

        // Heavy Fog & Condensation Risk based on relative humidity
        if (h >= 85.0f) {
            _data.fogRisk = 2; // High Fog / Dense Condensation Risk
        } else if (h >= 70.0f) {
            _data.fogRisk = 1; // Moderate Fog
        } else {
            _data.fogRisk = 0; // Low Fog
        }
    }
}

void SensorManager::readMPU() {
    if (!_data.mpuAvailable) {
        _data.pitchDeg = 0.0f;
        _data.rollDeg = 0.0f;
        _data.totalTiltDeg = 0.0f;
        _data.gForce = 1.0f;
        _data.tiltHazard = false;
        return;
    }

    sensors_event_t a, g, temp;
    if (_mpu.getEvent(&a, &g, &temp)) {
        _data.accelX = a.acceleration.x;
        _data.accelY = a.acceleration.y;
        _data.accelZ = a.acceleration.z;
        _data.gyroX = g.gyro.x;
        _data.gyroY = g.gyro.y;
        _data.gyroZ = g.gyro.z;

        float ax = a.acceleration.x;
        float ay = a.acceleration.y;
        float az = a.acceleration.z;

        // Compute Pitch & Roll in degrees
        _data.pitchDeg = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / PI;
        _data.rollDeg  = atan2(ay, az) * 180.0f / PI;
        _data.totalTiltDeg = sqrt(_data.pitchDeg * _data.pitchDeg + _data.rollDeg * _data.rollDeg);

        // G-Force magnitude
        float mag = sqrt(ax * ax + ay * ay + az * az);
        _data.gForce = mag / 9.80665f;

        float limit = configStore.get().tiltLimitDeg;
        _data.tiltHazard = (_data.totalTiltDeg > limit) || (az < -1.0f);
    }
}

void SensorManager::computeSensorFusion() {
    int score = 0;

    // 1. Fire / Thermal Threat (+45 max)
    if (_data.flameDetected) score += 45;

    // 2. Toxic Gas Threat (+35 max)
    if (_data.gasDetected) score += 35;

    // 3. Chassis Tilt / Rollover Threat (+30 max)
    if (_data.tiltHazard) {
        score += 30;
    } else if (_data.totalTiltDeg > 20.0f) {
        score += (int)((_data.totalTiltDeg - 20.0f) / 15.0f * 20.0f);
    }

    // 4. Obstacle Collision Threat (+25 max)
    if (_data.sonarResponding) {
        if (_data.proximityZone == 2) score += 25;
        else if (_data.proximityZone == 1) score += 10;
    }

    // 5. Environmental Extreme (+10 max)
    if (_data.temperature > 50.0f || _data.fogRisk == 2) score += 10;

    _data.threatScore = (uint8_t)constrain(score, 0, 100);

    // Threat Level Classification
    if (_data.threatScore >= 65) _data.threatLevel = 3; // CRITICAL
    else if (_data.threatScore >= 35) _data.threatLevel = 2; // WARNING
    else if (_data.threatScore >= 12) _data.threatLevel = 1; // ADVISORY
    else _data.threatLevel = 0; // NOMINAL
}

void SensorManager::update() {
    uint32_t now = millis();

    // Fast digital sampling
    readDigitalSensors();

    // Trigger ultrasonic pulse every 80ms (12.5Hz)
    if (now - _lastSonicTriggerMs >= 80) {
        _lastSonicTriggerMs = now;
        triggerUltrasonicPulse();
    }

    // Process echo timing (non-blocking)
    processUltrasonicEcho();

    // MPU6050 6-DOF IMU read every 40ms (25Hz)
    if (now - _lastMpuPollMs >= 40) {
        _lastMpuPollMs = now;
        readMPU();
    }

    // DHT11 read every 2000ms (0.5Hz)
    if (now - _lastDhtPollMs >= 2000) {
        _lastDhtPollMs = now;
        readDHT();
    }

    // Synthesize multi-sensor threat fusion matrix
    computeSensorFusion();

    _data.lastUpdateMs = now;
}
