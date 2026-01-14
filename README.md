# ESP32 RC Transmitter (ESP-NOW)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)

> RC transmitter based on ESP32 using ESP-NOW protocol for wireless transmission of 4 control channels.

## Overview

This project implements a professional RC transmitter using ESP32 microcontroller with ESP-NOW wireless protocol. It reads analog signals from ADS1115 ADC converter, handles buttons through PCF8574 I2C expander, and displays real-time data on SH1106 OLED screen.

## Features

- 📡 **4-channel RC transmission** via ESP-NOW protocol
- 🎚️ **16-bit precision** analog reading from ADS1115
- 🔘 **8 trim buttons** with hardware debouncing via PCF8574
- ⚙️ **Individual trims** for Throttle, Yaw, Pitch, and Roll
- 🖥️ **128x64 OLED display** (SH1106) for real-time monitoring
- 🎯 **Axis calibration** with configurable deadzone
- ⏱️ **Non-blocking** display updates
- 🔋 **Low latency** wireless communication

## Hardware Requirements

### Components

| Component | Description | Quantity |
|-----------|-------------|----------|
| ESP32 | Main microcontroller | 1 |
| ADS1115 | 16-bit ADC (I2C) | 1 |
| PCF8574 | I2C I/O expander | 1 |
| SH1106 | 128x64 OLED display (I2C) | 1 |
| Potentiometers | For joystick axes | 4 |
| Push buttons | For trim controls | 8 |

### Wiring Diagram

#### I2C Bus Configuration

All I2C devices share the same bus:

```
ESP32 GPIO 21 (SDA) ──┬── PCF8574 (SDA)
                      ├── ADS1115 (SDA)
                      └── SH1106 (SDA)

ESP32 GPIO 22 (SCL) ──┬── PCF8574 (SCL)
                      ├── ADS1115 (SCL)
                      └── SH1106 (SCL)
```

#### I2C Addresses

| Device | Address | Configurable |
|--------|---------|--------------|
| PCF8574 | `0x20` | Yes (A0-A2 pins) |
| ADS1115 | `0x48` | Yes (ADDR pin) |
| SH1106 | `0x3C` | Limited |

#### Analog Inputs (ADS1115)

| Channel | Function | Connected to |
|---------|----------|--------------|
| A0 | Throttle | Potentiometer/Joystick |
| A1 | Yaw | Potentiometer/Joystick |
| A2 | Pitch | Potentiometer/Joystick |
| A3 | Roll | Potentiometer/Joystick |

#### Digital Inputs (PCF8574)

| Pin | Button | Function |
|-----|--------|----------|
| P0 | BTN0 | Roll Trim + |
| P1 | BTN1 | Roll Trim - |
| P2 | BTN2 | Pitch Trim + |
| P3 | BTN3 | Pitch Trim - |
| P4 | BTN4 | Throttle Trim + |
| P5 | BTN5 | Throttle Trim - |
| P6 | BTN6 | Yaw Trim + |
| P7 | BTN7 | Yaw Trim - |

## Installation

### Prerequisites

Install the following libraries via Arduino Library Manager:

```
Adafruit ADS1X15
Adafruit PCF8574
Adafruit GFX Library
Adafruit SH110X
```

### Clone Repository

```bash
git clone https://github.com/yourusername/esp32-rc-transmitter.git
cd esp32-rc-transmitter
```

### Configuration

1. **Set Receiver MAC Address**

Edit the receiver MAC address in the code:

```cpp
uint8_t receiverAddress[] = {0x98, 0x88, 0xE0, 0xD1, 0x82, 0x3C};
```

To find your receiver's MAC address, use this code on the receiver ESP32:

```cpp
#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_MODE_STA);
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
}

void loop() {}
```

2. **Calibrate Axes**

Adjust calibration values based on your potentiometers:

```cpp
AxisCal axis[4] = {
  {12065, 15314, 13689},  // Throttle: {min, max, center}
  {11431, 14685, 13058},  // Yaw: {min, max, center}
  {12042, 15571, 13806},  // Pitch: {min, max, center}
  {11762, 15004, 13383}   // Roll: {min, max, center}
};
```

### Upload

1. Connect ESP32 via USB
2. Select board: **ESP32 Dev Module**
3. Select correct COM port
4. Click **Upload**

## Usage

### Data Protocol

The transmitter sends data using the following structure:

```cpp
struct RCData {
  int16_t throttle;  // 1000-2000
  int16_t yaw;       // 1000-2000
  int16_t pitch;     // 1000-2000
  int16_t roll;      // 1000-2000
};
```

**Channel Range:**
- Minimum: `1000`
- Center: `1500`
- Maximum: `2000`

### Trim Controls

| Control | Buttons | Step Size | Range |
|---------|---------|-----------|-------|
| Roll | 0 (↑) / 1 (↓) | ±5 | ±100 |
| Pitch | 2 (↑) / 3 (↓) | ±5 | ±100 |
| Throttle | 4 (↑) / 5 (↓) | ±5 | ±100 |
| Yaw | 6 (↑) / 7 (↓) | ±5 | ±100 |

### Display Information

The OLED screen shows:
- Real-time channel values
- Trim values for each axis
- Connection status
- Transmission rate

### Common Issues

**Issue:** No data transmission
- Check receiver MAC address
- Verify both devices are powered
- Ensure ESP-NOW is initialized on receiver

**Issue:** Erratic joystick readings
- Calibrate axes using serial monitor
- Check potentiometer connections
- Verify ADS1115 voltage reference

**Issue:** Display not working
- Check I2C address (use I2C scanner)
- Verify SDA/SCL connections
- Check display power supply (3.3V)

**Issue:** Buttons not responding
- Verify PCF8574 address
- Check pull-up resistors
- Test button continuity

## Contributing

Contributions are welcome! 

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Adafruit for excellent I2C libraries
- Espressif for ESP32 and ESP-NOW protocol
- Arduino community for support and examples

## Contact

Project Link: [https://github.com/yourusername/esp32-rc-transmitter](https://github.com/yourusername/esp32-rc-transmitter)

---

⭐ **Star this repository** if you find it helpful!
