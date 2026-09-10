#include "motors.h"

MotorController motors;

MotorController::MotorController()
    : _currentLeftPWM(0),
      _currentRightPWM(0),
      _currentMastAngle(90),
      _isArmed(false),
      _emergencyStop(false),
      _testModeActive(false),
      _testEndTimeMs(0) {}

void MotorController::begin() {
    // 1. Configure all direction GPIO pins as OUTPUT
    pinMode(PIN_MOTOR_IN1, OUTPUT);
    pinMode(PIN_MOTOR_IN2, OUTPUT);
    pinMode(PIN_MOTOR_IN3, OUTPUT);
    pinMode(PIN_MOTOR_IN4, OUTPUT);

    // Initial state: all direction pins LOW (stopped)
    digitalWrite(PIN_MOTOR_IN1, LOW);
    digitalWrite(PIN_MOTOR_IN2, LOW);
    digitalWrite(PIN_MOTOR_IN3, LOW);
    digitalWrite(PIN_MOTOR_IN4, LOW);

    // 2. Configure Dedicated LEDC Channels for Motor PWM (1 kHz, 8-bit)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PIN_MOTOR_ENA, PWM_MOTOR_FREQ, PWM_MOTOR_RES);
    ledcAttach(PIN_MOTOR_ENB, PWM_MOTOR_FREQ, PWM_MOTOR_RES);
    ledcWrite(PIN_MOTOR_ENA, 0);
    ledcWrite(PIN_MOTOR_ENB, 0);

    // 3. Configure Mast Servo on GPIO 23 (50 Hz, 14-bit)
    ledcAttach(PIN_SERVO_MAST, PWM_SERVO_FREQ, PWM_SERVO_RES);
#else
    // Left Motor: Channel 0 (Timer 0)
    ledcSetup(PWM_CHAN_MOTOR_L, PWM_MOTOR_FREQ, PWM_MOTOR_RES);
    ledcAttachPin(PIN_MOTOR_ENA, PWM_CHAN_MOTOR_L);
    ledcWrite(PWM_CHAN_MOTOR_L, 0);

    // Right Motor: Channel 1 (Timer 0)
    ledcSetup(PWM_CHAN_MOTOR_R, PWM_MOTOR_FREQ, PWM_MOTOR_RES);
    ledcAttachPin(PIN_MOTOR_ENB, PWM_CHAN_MOTOR_R);
    ledcWrite(PWM_CHAN_MOTOR_R, 0);

    // Mast Servo: Channel 2 (Timer 1)
    ledcSetup(PWM_CHAN_SERVO, PWM_SERVO_FREQ, PWM_SERVO_RES);
    ledcAttachPin(PIN_SERVO_MAST, PWM_CHAN_SERVO);
#endif

    setMastAngle(90); // Default centered mast position

    Serial.println("[MOTORS] L298N Dual Motor Driver & Servo initialized (Pure LEDC, zero timer conflicts).");
}

void MotorController::setLeftMotor(int speed) {
    speed = constrain(speed, -255, 255);
    _currentLeftPWM = speed;

    if (speed > 0) {
        digitalWrite(PIN_MOTOR_IN1, HIGH);
        digitalWrite(PIN_MOTOR_IN2, LOW);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcWrite(PIN_MOTOR_ENA, speed);
#else
        ledcWrite(PWM_CHAN_MOTOR_L, speed);
#endif
    } else if (speed < 0) {
        digitalWrite(PIN_MOTOR_IN1, LOW);
        digitalWrite(PIN_MOTOR_IN2, HIGH);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcWrite(PIN_MOTOR_ENA, -speed);
#else
        ledcWrite(PWM_CHAN_MOTOR_L, -speed);
#endif
    } else {
        digitalWrite(PIN_MOTOR_IN1, LOW);
        digitalWrite(PIN_MOTOR_IN2, LOW);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcWrite(PIN_MOTOR_ENA, 0);
#else
        ledcWrite(PWM_CHAN_MOTOR_L, 0);
#endif
    }
}

void MotorController::setRightMotor(int speed) {
    speed = constrain(speed, -255, 255);
    _currentRightPWM = speed;

    if (speed > 0) {
        digitalWrite(PIN_MOTOR_IN3, HIGH);
        digitalWrite(PIN_MOTOR_IN4, LOW);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcWrite(PIN_MOTOR_ENB, speed);
#else
        ledcWrite(PWM_CHAN_MOTOR_R, speed);
#endif
    } else if (speed < 0) {
        digitalWrite(PIN_MOTOR_IN3, LOW);
        digitalWrite(PIN_MOTOR_IN4, HIGH);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcWrite(PIN_MOTOR_ENB, -speed);
#else
        ledcWrite(PWM_CHAN_MOTOR_R, -speed);
#endif
    } else {
        digitalWrite(PIN_MOTOR_IN3, LOW);
        digitalWrite(PIN_MOTOR_IN4, LOW);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcWrite(PIN_MOTOR_ENB, 0);
#else
        ledcWrite(PWM_CHAN_MOTOR_R, 0);
#endif
    }
}

void MotorController::testMotor(int leftSpeed, int rightSpeed, uint32_t durationMs) {
    _testModeActive = true;
    _testEndTimeMs = millis() + durationMs;
    setLeftMotor(leftSpeed);
    setRightMotor(rightSpeed);
    Serial.printf("[MOTORS] Hardware Test Mode: Left=%d, Right=%d for %ums\n", leftSpeed, rightSpeed, durationMs);
}

void MotorController::update() {
    if (_testModeActive) {
        if (millis() >= _testEndTimeMs) {
            _testModeActive = false;
            stopAll();
            Serial.println("[MOTORS] Hardware Test Mode complete. Motors stopped.");
        }
    }
}

void MotorController::driveArcade(float throttle, float yaw, bool isArmed) {
    _isArmed = isArmed;

    // Do not override if emergency stop or test mode is running
    if (_testModeActive) return;

    if (!_isArmed || _emergencyStop) {
        stopAll();
        return;
    }

    // Arcade Mixing:
    // leftSpeed  = throttle + yaw
    // rightSpeed = throttle - yaw
    float left = throttle + yaw;
    float right = throttle - yaw;

    // Constrain to [-1.0, +1.0] range
    left = constrain(left, -1.0f, 1.0f);
    right = constrain(right, -1.0f, 1.0f);

    uint8_t maxSpeed = configStore.get().maxDriveSpeed;
    
    // Scale with minimum starting torque threshold to overcome L298N diode drop and gear friction
    int leftPWM = 0;
    if (abs(left) > 0.01f) {
        float sign = left > 0 ? 1.0f : -1.0f;
        leftPWM = (int)(sign * (MIN_MOTOR_PWM_TORQUE + abs(left) * (maxSpeed - MIN_MOTOR_PWM_TORQUE)));
        leftPWM = constrain(leftPWM, -255, 255);
    }

    int rightPWM = 0;
    if (abs(right) > 0.01f) {
        float sign = right > 0 ? 1.0f : -1.0f;
        rightPWM = (int)(sign * (MIN_MOTOR_PWM_TORQUE + abs(right) * (maxSpeed - MIN_MOTOR_PWM_TORQUE)));
        rightPWM = constrain(rightPWM, -255, 255);
    }

    setLeftMotor(leftPWM);
    setRightMotor(rightPWM);
}

void MotorController::stopAll() {
    _currentLeftPWM = 0;
    _currentRightPWM = 0;

    digitalWrite(PIN_MOTOR_IN1, LOW);
    digitalWrite(PIN_MOTOR_IN2, LOW);
    digitalWrite(PIN_MOTOR_IN3, LOW);
    digitalWrite(PIN_MOTOR_IN4, LOW);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PIN_MOTOR_ENA, 0);
    ledcWrite(PIN_MOTOR_ENB, 0);
#else
    ledcWrite(PWM_CHAN_MOTOR_L, 0);
    ledcWrite(PWM_CHAN_MOTOR_R, 0);
#endif
}

void MotorController::setMastAngle(int angle) {
    _currentMastAngle = constrain(angle, 0, 180);
    // 50Hz period = 20,000us. 14-bit resolution = 16384 ticks.
    // 0 deg -> 500us (~410 ticks), 180 deg -> 2500us (~2048 ticks)
    uint32_t pulseUs = 500 + ((uint32_t)_currentMastAngle * 2000) / 180;
    uint32_t duty = (pulseUs * 16384UL) / 20000UL;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PIN_SERVO_MAST, duty);
#else
    ledcWrite(PWM_CHAN_SERVO, duty);
#endif
}

void MotorController::setMastFromNormalized(float normValue) {
    int angle = (int)((normValue + 1.0f) * 0.5f * 180.0f);
    setMastAngle(angle);
}
