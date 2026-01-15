#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_PCF8574.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_ADS1X15.h>

// ---- KONFIGURACJA ESP-NOW ----
uint8_t receiverAddress[] = {0x98, 0x88, 0xE0, 0xD1, 0x82, 0x3C};

struct RCData {
  int16_t throttle;
  int16_t yaw;
  int16_t pitch;
  int16_t roll;
};

RCData myData;
esp_now_peer_info_t peerInfo;

// ---- PINY I ADRESY ----
#define PCF_ADDR 0x20
#define ADS_ADDR 0x48
#define OLED_ADDR 0x3C
// Piny enkodera
#define ENCODER_CLK 32
#define ENCODER_DT  33

volatile int encoderPos = 0;
int currentScreen = 0; // 0: MAIN, 1: TRIMS, 2: RATES
const int MAX_SCREENS = 3;

// Funkcja przerwania dla enkodera
void IRAM_ATTR readEncoder() {
  int dtValue = digitalRead(ENCODER_DT);
  if (dtValue == HIGH) encoderPos++;
  else encoderPos--;
}
Adafruit_PCF8574 pcf;
Adafruit_ADS1115 ads;
Adafruit_SH1106G display(128, 64, &Wire, -1);

// ---- USTAWIENIA KANAŁÓW ----
#define RC_MIN 1000
#define RC_MAX 2000
#define RC_CENTER 1500
#define DEADZONE_RC 30

enum { CH_THROTTLE = 0, CH_YAW = 1, CH_ROLL = 2, CH_PITCH = 3 };

// Kalibracja fizyczna drążków (Twoje wartości)
struct AxisCal { int min; int max; int center; bool invert; };
AxisCal axis[4] = {
  {12065, 15314, 13689, false}, // P0 - Throttle
  {11431, 14685, 13058, true},  // P1 - Yaw
  {12042, 15571, 13806, true},  // P2 - Roll
  {11762, 15004, 13383, true}   // P3 - Pitch
};

// ---- TRYMY I ZAKRESY (D/R) ----
int trim[4] = {0, 0, 0, 0};
float rates[4] = {1.0, 1.0, 1.0, 1.0}; // 1.0 = 100%

#define TRIM_STEP 5
#define TRIM_MAX  150
#define RATE_STEP 0.10
#define RATE_MIN  0.2
#define RATE_MAX  2.0
#define EXTENDED_MIN 500
#define EXTENDED_MAX 2500

// ---- TIMERY ----
unsigned long lastDisplayUpdate = 0;
unsigned long lastSendTime = 0;
const int displayInterval = 200;
const int sendInterval = 20; // 50Hz

// ---- DEBOUNCING ----
#define DEBOUNCE_DELAY 50
unsigned long lastDebounceTime[8] = {0};
bool lastButtonState[8] = {HIGH};
bool buttonState[8] = {HIGH};

// ---- LOGIKA PRZETWARZANIA SYGNAŁU ----

int processAxis(int channel) {
  int16_t raw = ads.readADC_SingleEnded(channel);
  AxisCal &c = axis[channel];

  // 1. Mapowanie surowe do zakresu 1000-2000
  raw = constrain(raw, c.min, c.max);
  long mapped = c.invert ? map(raw, c.min, c.max, RC_MAX, RC_MIN) : map(raw, c.min, c.max, RC_MIN, RC_MAX);

  // 2. Martwa strefa na środku
  if (abs(mapped - RC_CENTER) < DEADZONE_RC) mapped = RC_CENTER;

  // 3. Obliczanie wychylenia od środka
  int deflection = mapped - RC_CENTER;

  // 4. Aplikowanie Dual Rates (Zakresów) i Trimmerów
  int finalValue = RC_CENTER + (deflection * rates[channel]) + trim[channel];

  return constrain(finalValue, EXTENDED_MIN, EXTENDED_MAX); // Twardy limit bezpieczeństwa
}

void handleButtons(int button) {
  int ch = -1;
  int dir = 0;

  // Przypisanie przycisków do kanałów (zgodnie z Twoim pierwotnym kodem)
  if (button == 4) { ch = CH_THROTTLE; dir = 1;  }
  if (button == 5) { ch = CH_THROTTLE; dir = -1; }
  if (button == 6) { ch = CH_YAW;      dir = 1;  }
  if (button == 7) { ch = CH_YAW;      dir = -1; }
  if (button == 2) { ch = CH_PITCH;    dir = 1;  }
  if (button == 3) { ch = CH_PITCH;    dir = -1; }
  if (button == 0) { ch = CH_ROLL;     dir = 1;  }
  if (button == 1) { ch = CH_ROLL;     dir = -1; }

  if (ch != -1) {
      if (currentScreen == 1) {      // Ekran TRIMÓW
        trim[ch] = constrain(trim[ch] + (dir * TRIM_STEP), -TRIM_MAX, TRIM_MAX);
      } 
      else if (currentScreen == 2) { // Ekran DUAL RATES
        rates[ch] = constrain(rates[ch] + (dir * RATE_STEP), RATE_MIN, RATE_MAX);
      }
      // Na ekranie 0 (MAIN) przyciski mogą być zablokowane, by nic nie zmienić przypadkiem
    }
  }

void checkButtons() {
  for (uint8_t i = 0; i < 8; i++) {
    bool reading = pcf.digitalRead(i);
    if (reading != lastButtonState[i]) lastDebounceTime[i] = millis();
    if ((millis() - lastDebounceTime[i]) > DEBOUNCE_DELAY) {
      if (reading != buttonState[i]) {
        buttonState[i] = reading;
        if (buttonState[i] == LOW) handleButtons(i);
      }
    }
    lastButtonState[i] = reading;
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

void setup() {
  Serial.begin(115200);
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), readEncoder, FALLING);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_send_cb(onDataSent);
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  Wire.begin(21, 22);
  Wire.setClock(400000);
  pcf.begin(PCF_ADDR, &Wire);
  for (uint8_t i = 0; i < 8; i++) pcf.pinMode(i, INPUT_PULLUP);
  ads.begin(ADS_ADDR, &Wire);
  ads.setGain(GAIN_ONE);
  display.begin(OLED_ADDR, true);
}

void drawMainScreen() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  
  // === NAGŁÓWEK ===
  display.setTextSize(1);
  display.setCursor(10, 0);
  display.print("MAIN: 4CH SENDING");
  display.drawFastHLine(0, 10, 128, SH110X_WHITE);

  // === TREŚĆ ===
  display.setTextSize(1);
  display.setCursor(0, 15);
  display.printf("T:%4d R:%3d%% Tr:%+4d", myData.throttle, (int)(rates[0]*100), trim[0]);
  display.setCursor(0, 25);
  display.printf("Y:%4d R:%3d%% Tr:%+4d", myData.yaw,      (int)(rates[1]*100), trim[1]);
  display.setCursor(0, 35);
  display.printf("P:%4d R:%3d%% Tr:%+4d", myData.pitch,    (int)(rates[3]*100), trim[3]);
  display.setCursor(0, 45);
  display.printf("R:%4d R:%3d%% Tr:%+4d", myData.roll,     (int)(rates[2]*100), trim[2]);

  display.display();
}

void drawTrimScreen() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  
  // === NAGŁÓWEK ===
  display.setTextSize(1);
  display.setCursor(25, 0);
  display.print("EDIT: TRIMS");
  display.drawFastHLine(0, 10, 128, SH110X_WHITE);
  
  // === TREŚĆ ===
  display.setTextSize(1);
  display.setCursor(0, 15);
  display.printf("Throttle: %+4d", trim[CH_THROTTLE]);
  display.setCursor(0, 25);
  display.printf("Yaw:      %+4d", trim[CH_YAW]);
  display.setCursor(0, 35);
  display.printf("Pitch:    %+4d", trim[CH_PITCH]);
  display.setCursor(0, 45);
  display.printf("Roll:     %+4d", trim[CH_ROLL]);
  
  display.display();
}

void drawRateScreen() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  
  // === NAGŁÓWEK ===
  display.setTextSize(1);
  display.setCursor(15, 0);
  display.print("EDIT: DUAL RATES");
  display.drawFastHLine(0, 10, 128, SH110X_WHITE);
  
  // === TREŚĆ ===
  display.setTextSize(1);
  display.setCursor(0, 15);
  display.printf("Throttle: %3d%%", (int)(rates[CH_THROTTLE]*100));
  display.setCursor(0, 25);
  display.printf("Yaw:      %3d%%", (int)(rates[CH_YAW]*100));
  display.setCursor(0, 35);
  display.printf("Pitch:    %3d%%", (int)(rates[CH_PITCH]*100));
  display.setCursor(0, 45);
  display.printf("Roll:     %3d%%", (int)(rates[CH_ROLL]*100));
  
  display.display();
}

void loop() {
  unsigned long currentMillis = millis();
  
  currentScreen = ((encoderPos % MAX_SCREENS) + MAX_SCREENS) % MAX_SCREENS;  // ← POPRAWIONE
  checkButtons();

  if (currentMillis - lastSendTime >= sendInterval) {
    myData.throttle = processAxis(CH_THROTTLE);
    myData.yaw      = processAxis(CH_YAW);
    myData.roll     = processAxis(CH_ROLL);
    myData.pitch    = processAxis(CH_PITCH);
    esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));
    lastSendTime = currentMillis;
  }

  if (millis() - lastDisplayUpdate > displayInterval) {
    switch(currentScreen) {
      case 0: drawMainScreen(); break;
      case 1: drawTrimScreen(); break;
      case 2: drawRateScreen(); break;
    }
    // Usunięte duplikujące display.clearDisplay() i display.display()
    lastDisplayUpdate = millis();
  }
}