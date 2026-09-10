# ESP32 Mining Fog-Safety Rover — Firmware & Tactical Web HUD

A rugged, real-time safety and pilot rover designed for underground and open-pit mining operations in heavy fog and low-visibility conditions. It drives ahead of mine personnel, monitoring toxic gases, open flames, obstacle distances, and rollover attitude, streaming live telemetry over local WiFi to an operator's mobile HUD, while being driven via a **RadioMaster T8L** transmitter over ExpressLRS (**ELRS / CRSF protocol @ 420,000 baud**).

---

## 🛠️ Hardware Wiring & Pinout

All pins are pre-configured to match the existing rover wiring. **Do not modify pins in software unless re-soldering hardware.**

| Component / Subsystem | Pin Name | ESP32 Pin | Function / Description |
| :--- | :--- | :--- | :--- |
| **L298N Motor Driver** | ENA | `GPIO 13` | Left Motor Speed (PWM LEDC Ch 0, 20kHz) |
| | IN1 | `GPIO 12` | Left Motor Direction 1 |
| | IN2 | `GPIO 14` | Left Motor Direction 2 |
| | IN3 | `GPIO 27` | Right Motor Direction 1 |
| | IN4 | `GPIO 26` | Right Motor Direction 2 |
| | ENB | `GPIO 25` | Right Motor Speed (PWM LEDC Ch 1, 20kHz) |
| **IR Flame / Fire Sensor** | DO (Digital) | `GPIO 33` | Active LOW (`LOW` = Fire/Flame detected) |
| **DHT11 Sensor** | DATA | `GPIO 32` | Ambient Temperature (°C) & Humidity (%) |
| **MQ135 Gas Sensor** | DO (Digital) | `GPIO 35` | Active LOW (`LOW` = Toxic Gas / Smoke threshold exceeded) |
| **HC-SR04 Ultrasonic** | TRIG | `GPIO 5` | Ultrasonic Trigger Output (10µs pulse) |
| | ECHO | `GPIO 18` | Ultrasonic Echo Input (with 25ms timeout) |
| **Mast Tilt Servo** | PWM | `GPIO 23` | Camera / Sensor Mast Elevation (0° to 180°, 50Hz) |
| **MPU6050 Inclinometer** | SDA | `GPIO 21` | I2C Data (Pitch & Roll / Rollover detection) |
| | SCL | `GPIO 22` | I2C Clock (400kHz Fast Mode) |
| **ELRS CRSF Receiver** | TX (Receiver) | `GPIO 16` | ESP32 Serial2 RX2 (420,000 baud, 8N1) |
| | RX (Receiver) | `GPIO 17` | ESP32 Serial2 TX2 |

---

## ⚡ Flashing Instructions (PlatformIO)

The project consists of two parts: the **Firmware Binary** and the **LittleFS Filesystem Image** (Web HUD).

### 1. Build & Upload Firmware
Connect your ESP32 via USB and run:
```bash
pio run -t upload
```

### 2. Build & Upload Web Dashboard Assets (LittleFS)
To flash the tactical Web HUD HTML, CSS, and JS stored in `data/`:
```bash
pio run -t uploadfs
```

### 3. Open Serial Monitor
```bash
pio device monitor -b 115200
```

---

## 📡 Connecting to the Web HUD

1. Power on the rover. The ESP32 will automatically broadcast its SoftAP:
   - **Default SSID**: `MiningRover-Fog`
   - **Default Password**: `minehazard123`
2. Connect your smartphone, tablet, or laptop to `MiningRover-Fog`.
3. Open any web browser (Chrome, Safari, Firefox) and navigate to:
   ```
   http://192.168.4.1
   ```
4. The **Tactical Field HUD** will load immediately and stream live telemetry at 2Hz over WebSockets with zero page refreshes.

---

## 🎮 RadioMaster T8L & "Learn My Transmitter" Flow

The firmware features an interactive channel detection and calibration engine so you **never need to guess channel order manually**.

### Step-by-Step Learn Flow:
1. Turn on your **RadioMaster T8L** transmitter.
2. On the Web HUD, navigate to the **🎮 CONTROLLER MAPPING** tab.
3. You will see all **16 live CRSF channel bar meters** moving as you move your sticks and switches.
4. To assign a function (e.g. **THROTTLE**):
   - Click the **🎯 Detect** button next to Throttle.
   - The HUD will display: *"Move the stick or toggle the switch on your RadioMaster T8L now..."*
   - Move your left or right stick forward/backward.
   - The firmware identifies the active channel with the highest delta (|Δ| > 150), selects it, flashes the channel meter with a green glow, and automatically sets the assignment.
5. Repeat for **YAW (Steering)**, **MAST TILT (Servo)**, and **AUX (Arm / Safety Switch)**.
6. Toggle the **REV** switch if any control axis is inverted.
7. Click **💾 SAVE MAPPING TO NVS FLASH**. The settings are permanently stored in ESP32 non-volatile storage and survive power cycles.

---

## 🛡️ Safety Systems & Failsafe

- **CRSF Watchdog & Failsafe**: If no valid CRSF packet is received on Serial2 for **>500ms** (e.g. transmitter out of range, turned off, or receiver disconnected), the firmware immediately enters **Failsafe Mode**:
  - Both L298N motors are immediately cut to 0 PWM and all directional pins are driven LOW.
  - A prominent blinking banner **"ELRS SIGNAL LOSS: FAILSAFE ACTIVE"** appears on the Web HUD.
- **Critical Hazard Alarms**:
  - 🔥 **Fire / Flame Alert**: Triggered when IR sensor on `GPIO33` goes `LOW`.
  - ☣️ **Toxic Gas Alert**: Triggered when MQ135 sensor on `GPIO35` goes `LOW`.
  - ⚠️ **Chassis Rollover Warning**: Triggered when MPU6050 pitch/roll tilt exceeds the user-configured angle threshold (default 35°) or chassis is inverted (`az < -1.0 m/s²`).
- **Obstacle Proximity Brake**: If the HC-SR04 ultrasonic sensor detects an obstacle within the safety threshold (default 15cm), forward drive is inhibited while still allowing reverse/turning.
- **Web HUD Emergency Stop**: Clicking **🛑 E-STOP** on the HUD header immediately halts all motor motion.

---

## 🌐 REST & WebSocket API Reference

### WebSocket Endpoint: `ws://192.168.4.1/ws`
Streams 2Hz JSON telemetry packets:
```json
{
  "type": "telemetry",
  "link": { "connected": true, "failsafe": false, "age_ms": 22, "valid_frames": 1420, "crc_errors": 0 },
  "sensors": {
    "temp": 26.4, "humidity": 65.0, "dht_ok": true,
    "flame": false, "gas": false,
    "distance": 145.2, "obstacle": false,
    "pitch": 2.1, "roll": -0.8, "tilt": 2.2, "tilt_hazard": false, "mpu_ok": true
  },
  "rover": { "armed": true, "e_stop": false, "left_pwm": 180, "right_pwm": 180, "mast_angle": 90 },
  "channels": [ 992, 992, 1450, 992, 1811, 992, 172, 992, 992, 992, 992, 992, 992, 992, 992, 992 ]
}
```

### REST Endpoints
- `GET /api/status`: Rover link and operational status.
- `GET /api/channels`: 16 raw CRSF channel values + percentage positions.
- `GET /api/sensors`: Environmental and hazard sensor readings.
- `GET /api/config`: Current channel mapping, deadbands, and WiFi parameters.
- `POST /api/config/mapping`: Update and persist channel assignments to NVS.
- `POST /api/config/wifi`: Configure uplink credentials for site WiFi (`sta_ssid`, `sta_pass`).
- `POST /api/emergency_stop`: Software motor kill switch toggle.
- `POST /api/learn/baseline`: Capture baseline for transmitter learn flow.
- `POST /api/learn/detect`: Identify active moving channel.

---

## 📁 Project Structure

```
Esp_car/
├── include/
│   └── config.h              # Hardware pinouts, PWM frequencies, timing constants
├── src/
│   ├── config_store.h/.cpp   # NVS Preferences storage for channel mapping & WiFi
│   ├── crsf.h/.cpp           # ELRS CRSF protocol parser (16 channels, CRC8-D5)
│   ├── motors.h/.cpp         # L298N arcade mixer and mast servo controller
│   ├── sensors.h/.cpp        # Non-blocking DHT11, MQ135, Flame, HC-SR04, MPU6050
│   ├── web_server.h/.cpp     # AsyncWebServer, WebSocket broadcaster & REST APIs
│   └── main.cpp              # System initialization and high-speed control loop
├── data/
│   ├── index.html            # Tactical industrial field HUD interface
│   ├── style.css             # High-contrast dark theme & responsive HUD styles
│   └── app.js                # WebSocket client, SVG gauges & Learn Flow engine
├── platformio.ini            # PlatformIO environment, LittleFS & library dependencies
└── README.md                 # Complete documentation & wiring guide
```
