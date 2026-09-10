#pragma once

#include <Arduino.h>
#include "config.h"
#include "config_store.h"

class MotorController {
public:
    MotorController();

    void begin();
    void update(); // Handles test mode timeouts

    // Drive arcade mixing: throttle (-1.0 to 1.0), yaw (-1.0 to 1.0)
    void driveArcade(float throttle, float yaw, bool isArmed);

    // Hardware Test Mode: runs motors at specific speeds for a duration
    void testMotor(int leftSpeed, int rightSpeed, uint32_t durationMs = 2000);

    // Direct motor speed controls (-255 to +255)
    void setLeftMotor(int speed);
    void setRightMotor(int speed);

    // Stop all drive immediately (used on failsafe, emergency stop, or disarm)
    void stopAll();

    // Set Mast Tilt Angle (0 to 180 degrees)
    void setMastAngle(int angle);

    // Set Mast from normalized channel (-1.0 to 1.0 or 0.0 to 1.0)
    void setMastFromNormalized(float normValue);

    // Actuator telemetry getters
    int getLeftPWM() const { return _currentLeftPWM; }
    int getRightPWM() const { return _currentRightPWM; }
    int getMastAngle() const { return _currentMastAngle; }
    bool isArmed() const { return _isArmed; }
    bool isEmergencyStopped() const { return _emergencyStop; }
    bool isTestModeActive() const { return _testModeActive; }

    void setEmergencyStop(bool stop) { _emergencyStop = stop; if (stop) stopAll(); }

private:
    int _currentLeftPWM;
    int _currentRightPWM;
    int _currentMastAngle;
    bool _isArmed;
    bool _emergencyStop;

    bool _testModeActive;
    uint32_t _testEndTimeMs;
};

extern MotorController motors;
