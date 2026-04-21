# 📘 PROJECT BIBLE: SparkFun IoT RedBoard ESP32

## 🛑 HARDWARE RULES
1. **MEAS JUMPER:** Must be **OPEN** (Cut/Clear). Bridging it while on USB back-feeds 5V into the logic rail, crashing I2C.
2. **GPIO 5:** This is a dual-purpose pin. It powers the Qwiic bus. It MUST be `digitalWrite(5, HIGH)` to see any sensors.
3. **SD CARD:** Built-in slot uses GPIO 5 as Chip Select. Accessing SD briefly interrupts peripheral power. Sensor reads must finish *before* SD writes.

## 🛰️ I2C DEVICE INVENTORY
| Address | Device | Function |
| :--- | :--- | :--- |
| **0x28** | XENSIV PAS CO2 | Air Quality |
| **0x32** | RV-8803 RTC | Timekeeping |
| **0x36** | MAX17048 | Battery Voltage & % |
| **0x3C** | SSD1306 OLED | Screen |

## 🔋 BATTERY LOGIC
- **Chip:** MAX17048 Fuel Gauge.
- **Protocol:** I2C only. 
- **Command:** `lipo.getVoltage()` and `lipo.getSOC()`.
- **Note:** Disregard any code mentioning `analogRead(34)`.