#include <Wire.h>
#include <Adafruit_PCF8574.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_ADS1X15.h>

// ---- I2C addresses ----
#define PCF_ADDR 0x20
#define ADS_ADDR 0x48
#define OLED_ADDR 0x3C

// ---- Objects ----
Adafruit_PCF8574 pcf;
Adafruit_ADS1115 ads;
Adafruit_SH1106G display(128, 64, &Wire, -1);

#define TRIM_STEP 5
#define TRIM_MAX 100

int trim[4] = {0, 0, 0, 0};

enum {
 CH_THROTTLE = 0,
 CH_YAW   = 1,
 CH_ROLL   = 2,
 CH_PITCH  = 3
};

struct AxisCal {
 int min;
 int max;
 int center;
};

AxisCal axis[4] = {
 {12065, 15314, 13689}, // P0
 {11431, 14685, 13058}, // P1
 {12042, 15571, 13806}, // P2
 {11762, 15004, 13383} // P3
};

#define RC_MIN   1000
#define RC_MAX   2000
#define RC_CENTER 1500
#define DEADZONE_RC 75

int mapAxis(int raw, AxisCal &c, bool invert = false) {
 raw = constrain(raw, c.min, c.max);

 long mapped;
 if (!invert)
  mapped = map(raw, c.min, c.max, RC_MIN, RC_MAX);
 else
  mapped = map(raw, c.min, c.max, RC_MAX, RC_MIN);

 // deadzone w skali RC
 if (abs(mapped - RC_CENTER) < DEADZONE_RC)
  mapped = RC_CENTER;

 return mapped;
}

void handleTrim(int button) {
 switch (button) {
  case 4: trim[CH_THROTTLE] += TRIM_STEP; break;
  case 5: trim[CH_THROTTLE] -= TRIM_STEP; break;
  case 6: trim[CH_YAW]   += TRIM_STEP; break;
  case 7: trim[CH_YAW]   -= TRIM_STEP; break;
  case 2: trim[CH_PITCH]  += TRIM_STEP; break;
  case 3: trim[CH_PITCH]  -= TRIM_STEP; break;
  case 0: trim[CH_ROLL]   += TRIM_STEP; break;
  case 1: trim[CH_ROLL]   -= TRIM_STEP; break;
 }

 for (int i = 0; i < 4; i++)
  trim[i] = constrain(trim[i], -TRIM_MAX, TRIM_MAX);
}

void setup() {
 Serial.begin(115200);
 delay(200);

 Wire.begin(21, 22);
 Wire.setClock(100000);

 // ---- PCF8574 ----
 if (!pcf.begin(PCF_ADDR, &Wire)) {
  Serial.println("❌ PCF8574 not found");
  while (1);
 }
 for (uint8_t i = 0; i < 8; i++) pcf.pinMode(i, INPUT_PULLUP);

 // ---- ADS1115 ----
 if (!ads.begin(ADS_ADDR, &Wire)) {
  Serial.println("❌ ADS1115 not found");
  while (1);
 }
 ads.setGain(GAIN_ONE); // ±4.096V

 // ---- OLED ----
 if (!display.begin(OLED_ADDR, true)) {
  Serial.println("❌ OLED not found");
  while (1);
 }

 display.clearDisplay();
 display.setTextColor(SH110X_WHITE);
 display.setCursor(0, 0);
 display.setTextSize(1);
 display.println("System ready");
 display.display();
 delay(1000);
}

void loop() {
 int pressedButton = -1;

 int16_t raw[4];
 int16_t rc[4];

 raw[0] = ads.readADC_SingleEnded(0); // LY
 raw[1] = ads.readADC_SingleEnded(1); // LX
 raw[2] = ads.readADC_SingleEnded(2); // RX
 raw[3] = ads.readADC_SingleEnded(3); // RY

 rc[CH_THROTTLE] = constrain(mapAxis(raw[0], axis[0], false) + trim[CH_THROTTLE], RC_MIN, RC_MAX);
 rc[CH_YAW]   = constrain(mapAxis(raw[1], axis[1], true ) + trim[CH_YAW],   RC_MIN, RC_MAX);
 rc[CH_ROLL]   = constrain(mapAxis(raw[2], axis[2], true ) + trim[CH_ROLL],   RC_MIN, RC_MAX);
 rc[CH_PITCH]  = constrain(mapAxis(raw[3], axis[3], true ) + trim[CH_PITCH],  RC_MIN, RC_MAX);

 for (uint8_t i = 0; i < 8; i++) {
  if (pcf.digitalRead(i) == LOW) {
   pressedButton = i;
   handleTrim(i);
   delay(150); // proste zabezpieczenie przed spamem
   break;
  }
 }
 display.clearDisplay();
 display.setTextColor(SH110X_WHITE);

 // 1. NAGŁÓWEK
 display.setTextSize(1);
 display.setCursor(32, 0);
 display.print("- RC STATUS -");
 display.drawFastHLine(0, 10, 128, SH110X_WHITE); // Linia pod nagłówkiem

 // 2. OSIEL (RC VALUES) - Układ w dwóch kolumnach dla lepszej czytelności
 display.setTextSize(1);

 // Kolumna lewa (Throttle / Yaw)
 display.setCursor(0, 18);
 display.print("THR: "); display.print(rc[CH_THROTTLE]);
 display.setCursor(0, 28);
 display.print("YAW: "); display.print(rc[CH_YAW]);

 // Kolumna prawa (Pitch / Roll)
 display.setCursor(68, 18);
 display.print("PIT: "); display.print(rc[CH_PITCH]);
 display.setCursor(68, 28);
 display.print("ROL: "); display.print(rc[CH_ROLL]);

 // 3. SEPARATOR
 display.drawFastHLine(0, 42, 128, SH110X_WHITE);

 // 4. TRIMY (Mniejsza czcionka, na dole)
 display.setCursor(0, 48);
 display.print("TRIM SETTINGS:");

 // Rozmieszczenie trimów w jednym rzędzie
 display.setCursor(0, 57);
 display.print("T:"); display.print(trim[CH_THROTTLE]);
 display.print(" Y:"); display.print(trim[CH_YAW]);
 display.print(" P:"); display.print(trim[CH_PITCH]);
 display.print(" R:"); display.print(trim[CH_ROLL]);

 display.display();
 delay(100);
}