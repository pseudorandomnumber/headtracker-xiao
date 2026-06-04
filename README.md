# HeadTracker — Seeed XIAO ESP32C6 Port

A head tracking device for FPV goggles, built on the **Seeed XIAO ESP32C6** with a **GY-BNO055** IMU.

Inspired by and protocol-compatible with the excellent **[HeadTracker project by dlktdr/Cliff Blackburn](https://github.com/headtracker/HeadTracker)** (GPL-3.0).

---

## Features

- **9-DOF sensor fusion** via BNO055 built-in AHRS (no drift on Pan/Yaw thanks to magnetometer)
- **PPM output** — 8 channels on 3.5mm trainer jack (GPIO2), ~20ms frame at 50Hz
- **BLE HID Joystick** — wireless trainer via Bluetooth LE (EdgeTX Radiomaster Zorro / BetaFPV)
- **WiFi Web Configurator** — connect to the ESP32's WiFi AP and configure via browser
- **SSD1306 OLED** status display (0.91" 128×32 or 0.66" 64×48)
- **One-button center** — short press = zero all offsets, long press = cycle screens
- **Joystick navigation** — Deek-Robot dual-axis joystick for screen cycling
- **Persistent settings** — saved to NVS flash via `Preferences`
- **USB Serial configurator** — compatible with the [HeadTracker-configurator](https://github.com/headtracker/HeadTracker-configurator) Angular app (Web Serial API)

---

## Hardware

| Part | Notes |
|---|---|
| [Seeed XIAO ESP32C6](https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32C6-p-5884.html) | Main MCU |
| GY-BNO055 | 9-DOF IMU with onboard sensor fusion |
| SSD1306 0.91" OLED (128×32) | Status display (or 0.66" 64×48) |
| Momentary push button | Center/zero button |
| Deek-Robot dual-axis joystick | Screen navigation |
| 3.5mm mono audio jack | PPM trainer output |

---

## Wiring

```
XIAO ESP32C6          GY-BNO055 / SSD1306
─────────────         ───────────────────
3.3V            ──→   VCC  (both)
GND             ──→   GND  (both)
D4 / GPIO6      ──→   SDA  (shared I2C bus)
D5 / GPIO7      ──→   SCL  (shared I2C bus)
                       BNO055 ADR → GND  (I2C address 0x28)
                       SSD1306 addr 0x3C (default)

D0 / GPIO2      ──→   PPM out → 3.5mm tip → trainer jack
D1 / GPIO3      ──→   Center button → GND (internal pull-up)
D2 / GPIO4      ──→   Joystick VRX (ADC)
D3 / GPIO5      ──→   Joystick VRY (ADC)
                       Joystick GND → GND, VCC → 3.3V
```

**3.5mm trainer jack:** Tip = PPM signal, Sleeve = GND

---

## Build & Flash

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)

### Steps

```bash
# 1. Clone this repo
git clone https://github.com/YOUR_USERNAME/headtracker-xiao.git
cd headtracker-xiao

# 2. Flash the LittleFS web UI files first
pio run --target uploadfs

# 3. Build and flash the firmware
pio run --target upload

# 4. Open serial monitor (optional)
pio device monitor
```

> **Note:** On first use, run `uploadfs` before `upload` so the web UI files are on the device.

---

## Usage

### WiFi Web Configurator (primary)

1. Power on the HeadTracker
2. On your phone or PC, connect to WiFi network **`HeadTracker-XXXX`** (no password)
3. Open browser → navigate to **`http://192.168.4.1`**
4. The web configurator loads with 5 tabs:
   - **Live** — real-time angle + channel output display
   - **Channels** — configure Pan/Tilt/Roll: min/center/max/gain/channel/reverse
   - **Output** — enable/disable PPM and BLE HID
   - **Calibration** — BNO055 calibration status guide
   - **System** — save to flash, reboot, firmware info

### USB Serial Configurator (optional)

Connect via USB, then use the [HeadTracker-configurator](https://github.com/headtracker/HeadTracker-configurator) Angular app (runs in Chrome/Edge with Web Serial API).

The same JSON protocol runs over both WebSocket and USB Serial.

### Physical Controls

| Action | Result |
|---|---|
| Short press button (< 800ms) | Zero all axes (set center) |
| Long press button (≥ 800ms) | Cycle OLED screen |
| Joystick up | Next OLED screen |
| Joystick down | Previous OLED screen |

---

## OLED Screens

| Screen | Content |
|---|---|
| 0 – Live | Pan/Tilt/Roll angles + µs values |
| 1 – Calibration | BNO055 Sys/Gyro/Accel/Mag calibration levels |
| 2 – Status | Firmware version, PPM/BLE state, WiFi SSID |

---

## Channel Mapping

```
output_µs = center_µs + direction × angle_deg × gain
output_µs = constrain(output_µs, min_µs, max_µs)

direction = reversed ? -1 : +1
```

Default channel assignments:
- **CH1** = Tilt
- **CH2** = Roll
- **CH3** = Pan

---

## BLE Wireless Trainer (EdgeTX)

1. In EdgeTX: **Model Setup → Trainer → BT Joystick**
2. The XIAO appears as **"HeadTracker"** in the BT device list
3. Connect — head movement maps to channels

> Note: Tested with Radiomaster Zorro and BetaFPV remotes running EdgeTX.

---

## PPM Wired Trainer

Connect **GPIO2 → 3.5mm tip** to your remote's trainer port.
- In EdgeTX: **Model Setup → Trainer → Master/Wired**
- 8 channels, 50Hz frame rate, positive PPM polarity

---

## Sensor Mounting

The GY-BNO055 should be mounted **flat, face-up** on top of the FatShark goggles. The firmware uses the BNO055 default P1 orientation:

| Axis | BNO055 Output | Maps to |
|---|---|---|
| Heading (0–360°) | Pan/Yaw | PPM CH3 |
| Roll (±90°) | Roll | PPM CH2 |
| Pitch (±180°) | Tilt | PPM CH1 |

If your physical mount is different, adjust the axis assignments in `sense.cpp`.

---

## Configuration Persistence

Settings are saved to ESP32 NVS flash (Preferences library). To save your configuration, use the **"Save to Flash"** button in the System tab or send `{"Cmd":"Flash"}` over serial.

---

## Serial Protocol

The firmware speaks the same JSON protocol as the original HeadTracker project:

```json
→ {"Cmd":"Get"}                          // Request all settings
← {"Cmd":"Set","PanMin":1000,"PanMax":2000,...}  // Settings response

→ {"Cmd":"Set","PanGain":1.5}            // Update a setting
← {"Cmd":"Set",...}                      // Echo updated settings

→ {"Cmd":"RstCnt"}                       // Reset center
→ {"Cmd":"Flash"}                        // Save to flash
→ {"Cmd":"Reboot"}                       // Reboot device

→ {"Cmd":"RD","panout":true,"tiltout":true}  // Start data stream
← {"Cmd":"Data","panout":1512,...}           // Real-time data

→ {"Cmd":"D--"}                          // Stop data stream
```

---

## Acknowledgements

This project is heavily inspired by the **[HeadTracker project](https://github.com/headtracker/HeadTracker)** by Cliff Blackburn (dlktdr), licensed under GPL-3.0. The JSON communication protocol and channel mapping concepts are derived from that project.

- Original firmware: https://github.com/headtracker/HeadTracker
- Web configurator: https://github.com/headtracker/HeadTracker-configurator
- Documentation: https://headtracker.gitbook.io/head-tracker-v2.2

---

## License

GPL-3.0 — see original project for full license text.
