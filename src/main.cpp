#include <Arduino.h>
#include "config.h"
#include "config_store.h"
#include "crsf.h"
#include "motors.h"
#include "sensors.h"
#include "web_server.h"

// Last status print timestamp for serial monitor
static uint32_t lastSerialLogMs = 0;

void setup() {
    // 1. Initialize Debug Serial Monitor
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("=================================================");
    Serial.println("   ESP32 MINING FOG-SAFETY ROVER FIRMWARE");
    Serial.println("   ELRS CRSF 420K | L298N | MPU6050 | WEB HUD");
    Serial.println("=================================================");

    // 2. Initialize Persistent Storage (NVS / Preferences)
    configStore.begin();
    Serial.println("[SYSTEM] Configuration loaded from NVS.");

    // 3. Initialize Motors and Mast Tilt Servo
    motors.begin();
    Serial.println("[SYSTEM] Motor driver and mast servo initialized.");

    // 4. Initialize Environmental and Hazard Sensors (Non-blocking)
    sensors.begin();
    Serial.println("[SYSTEM] Environmental sensors initialized.");

    // 5. Initialize CRSF Receiver on Serial2 (Pins 16/17 @ 420000 baud)
    crsf.begin(Serial2, CRSF_BAUDRATE, PIN_CRSF_RX, PIN_CRSF_TX);
    Serial.printf("[SYSTEM] CRSF receiver listening on Serial2 (RX=GPIO%d, TX=GPIO%d) @ %d baud.\n", 
                  PIN_CRSF_RX, PIN_CRSF_TX, CRSF_BAUDRATE);

    // 6. Initialize WiFi, LittleFS, and Async Web Server
    webServer.begin();
    Serial.println("[SYSTEM] System initialization complete. Entering high-speed control loop.");
}

void loop() {
    // 1. Non-blocking CRSF packet parser (Reads incoming packets immediately)
    crsf.update();

    // 2. Non-blocking sensor polling & hazard calculation
    sensors.update();

    // 3. Rover Drive & Actuator Control Engine
    const RoverConfig &cfg = configStore.get();

    if (crsf.isFailsafe()) {
        // FAILSAFE: Signal lost for >500ms -> immediately cut drive
        motors.stopAll();
    } else {
        // Determine Arming State based on configured Aux Switch mode
        bool isArmed = true;
        uint16_t auxRaw = crsf.getRawChannel(cfg.auxChannel);

        if (cfg.auxMode == AUX_MODE_ARM_DISARM) {
            // High position (> 1400) = Armed, Low position (< 1400) = Disarmed
            isArmed = (auxRaw > 1400);
        } else if (cfg.auxMode == AUX_MODE_RETURN_IDLE) {
            // Low position (< 1400) = Normal, High position (> 1400) = Force Idle
            isArmed = (auxRaw <= 1400);
        } else {
            // AUX_MODE_ALWAYS_ARMED (3) or AUX_MODE_HEADLAMP_TOGGLE (1): Auto-armed with transmitter link
            isArmed = true;
        }

        // Compute Normalized Throttle and Yaw (-1.0 to +1.0)
        float throttle = crsf.getNormalized(cfg.throttleChannel, cfg.invertThrottle, cfg.deadband);
        float yaw = crsf.getNormalized(cfg.yawChannel, cfg.invertYaw, cfg.deadband);

        // Safety Obstacle Brake (Only active if explicitly enabled by operator)
        const SensorData &sd = sensors.getData();
        if (cfg.enableObstacleBrake && sd.obstacleWarning && throttle > 0.0f) {
            throttle = 0.0f; // Block forward motion into immediate obstacle
        }

        // Apply drive commands to L298N differential motors
        motors.driveArcade(throttle, yaw, isArmed);

        // Update Camera / Sensor Mast Tilt Servo
        float tiltNorm = crsf.getNormalized(cfg.tiltChannel, cfg.invertTilt, 0);
        motors.setMastFromNormalized(tiltNorm);
    }

    // 4. Update Motor Hardware Test timers
    motors.update();

    // 5. WebSocket Telemetry Broadcaster & Web Server Maintenance
    webServer.update();

    // 5. Periodic Serial Diagnostic Log (1 Hz)
    uint32_t now = millis();
    if (now - lastSerialLogMs >= 1000) {
        lastSerialLogMs = now;
        const SensorData &sd = sensors.getData();
        Serial.printf("[DIAG] CRSF: %s (age %ums, valid:%u, err:%u) | Armed:%d | L:%d R:%d | Dist:%.1fcm | Tilt:%.1f°\n",
                      crsf.isConnected() ? "OK" : "FAILSAFE",
                      crsf.getFrameAgeMs(),
                      crsf.getValidFrameCount(),
                      crsf.getCrcErrorCount(),
                      motors.isArmed(),
                      motors.getLeftPWM(),
                      motors.getRightPWM(),
                      sd.distanceCm,
                      sd.totalTiltDeg);
    }
}
