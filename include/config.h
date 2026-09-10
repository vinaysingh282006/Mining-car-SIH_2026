#pragma once

#include <Arduino.h>

// =============================================================================
// HARDWARE PIN DEFINITIONS
// DO NOT MODIFY THESE PINS - Matched to current rover hardware wiring
// =============================================================================

// L298N Differential Motor Driver Pins
#define PIN_MOTOR_ENA       13   // Left Motor PWM Speed Enable
#define PIN_MOTOR_IN1       12   // Left Motor Direction 1
#define PIN_MOTOR_IN2       14   // Left Motor Direction 2
#define PIN_MOTOR_IN3       27   // Right Motor Direction 1
#define PIN_MOTOR_IN4       26   // Right Motor Direction 2
#define PIN_MOTOR_ENB       25   // Right Motor PWM Speed Enable

// Environmental & Hazard Sensors
#define PIN_FLAME           33   // IR Flame Sensor (Active LOW: LOW = Fire Detected)
#define PIN_DHT             32   // DHT11 Temperature & Humidity Sensor
#define PIN_GAS             35   // MQ135 Gas/Smoke Sensor (Active LOW: LOW = Gas Exceeded)
#define PIN_TRIG             5   // HC-SR04 Ultrasonic Trigger
#define PIN_ECHO            18   // HC-SR04 Ultrasonic Echo

// Actuators
#define PIN_SERVO_MAST      23   // Camera/Sensor Mast Tilt Servo (PWM)

// I2C Bus for MPU6050 Inclinometer / IMU
#define PIN_I2C_SDA         21   // I2C SDA
#define PIN_I2C_SCL         22   // I2C SCL

// ELRS CRSF Serial2 Interface
#define PIN_CRSF_RX         16   // ESP32 RX2 (Connected to Receiver TX)
#define PIN_CRSF_TX         17   // ESP32 TX2 (Connected to Receiver RX)
#define CRSF_BAUDRATE   420000   // ELRS standard CRSF baud rate

// =============================================================================
// PWM CHANNELS & FREQUENCIES (ESP32 LEDC)
// =============================================================================
// 1000 Hz (1 kHz) optimal for L298N bipolar transistors to deliver maximum torque
#define PWM_MOTOR_FREQ    1000   // 1 kHz motor PWM
#define PWM_MOTOR_RES        8   // 8-bit resolution (0 - 255)
#define PWM_CHAN_MOTOR_L     0   // LEDC Channel 0 for Left Motor (ENA - GPIO 13)
#define PWM_CHAN_MOTOR_R     1   // LEDC Channel 1 for Right Motor (ENB - GPIO 25)
#define PWM_CHAN_SERVO       2   // LEDC Channel 2 for Mast Servo (GPIO 23)
#define PWM_SERVO_FREQ      50   // 50 Hz standard servo frequency
#define PWM_SERVO_RES       14   // 14-bit resolution (0 - 16383)
#define MIN_MOTOR_PWM_TORQUE 65  // Minimum starting PWM to overcome L298N voltage drop & motor inertia

// =============================================================================
// CRSF PROTOCOL SPECIFICATIONS
// =============================================================================
#define CRSF_CHANNEL_MIN    172  // ~1000us nominal end
#define CRSF_CHANNEL_MID    992  // ~1500us center
#define CRSF_CHANNEL_MAX   1811  // ~2000us nominal end
#define CRSF_NUM_CHANNELS    16  // 16 RC channels in CRSF packed frame
#define CRSF_FAILSAFE_MS    500  // Loss of signal threshold (ms)

// Default Channel Mappings (0-indexed)
#define DEFAULT_CH_THROTTLE   2  // Channel 3 (Mode 2 Throttle/Pitch)
#define DEFAULT_CH_YAW        3  // Channel 4 (Mode 2 Yaw/Roll)
#define DEFAULT_CH_TILT       5  // Channel 6 (Potentiometer / Tilt Knob)
#define DEFAULT_CH_AUX        4  // Channel 5 (Switch SB / Arm / Aux)

// Auxiliary Channel Modes
enum AuxMode {
    AUX_MODE_ARM_DISARM = 0,     // Switch HIGH (>1400) = Armed, LOW (<1400) = Disarmed
    AUX_MODE_HEADLAMP_TOGGLE = 1,// Always Armed; switch toggles aux feature
    AUX_MODE_RETURN_IDLE = 2,    // Switch HIGH (>1400) = Emergency Idle Stop
    AUX_MODE_ALWAYS_ARMED = 3    // Auto-Arm whenever CRSF link is active
};

// =============================================================================
// WIFI & SYSTEM DEFAULTS
// =============================================================================
#define DEFAULT_AP_SSID     "MiningRover-Fog"
#define DEFAULT_AP_PASS     "minehazard123"
#define DEFAULT_AP_CHANNEL  1

// Telemetry streaming interval over WebSocket (ms)
#define TELEMETRY_INTERVAL_MS   500  // 2 Hz telemetry broadcast
