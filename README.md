# 🎮 ESP32 RC Transmitter (ESP-NOW)

Nadajnik RC oparty o ESP32 wykorzystujący protokół ESP-NOW do bezprzewodowej transmisji 4 kanałów sterujących.  
Odczytuje sygnały analogowe z przetwornika ADS1115, obsługuje przyciski przez PCF8574 i wyświetla dane na ekranie OLED SH1106.

---

## ✨ Funkcje

- 📡 Transmisja 4 kanałów RC przez ESP-NOW
- 🎚 Odczyt 4 osi z ADS1115 (16-bit)
- 🔘 8 przycisków trymerów przez PCF8574 (z debouncingiem)
- ⚙️ Trymery dla Throttle, Yaw, Pitch i Roll
- 🖥 Wyświetlacz OLED SH1106 128x64
- 🎯 Kalibracja osi oraz martwa strefa (deadzone)
- ⏱ Nieblokująca obsługa wyświetlacza

---

## 🧩 Wymagany sprzęt

- ESP32
- ADS1115 (I2C)
- PCF8574 (I2C)
- OLED SH1106 128x64 (I2C)
- 4x potencjometr / joystick
- 8x przycisk

---

## 🔌 Połączenia I2C

| Urządzenie | Adres |
|------------|--------|
| PCF8574    | 0x20   |
| ADS1115    | 0x48   |
| OLED SH1106| 0x3C   |

I2C:
- SDA → GPIO 21  
- SCL → GPIO 22  

---

## 📡 Struktura danych

```cpp
struct RCData {
  int16_t throttle;
  int16_t yaw;
  int16_t pitch;
  int16_t roll;
};
