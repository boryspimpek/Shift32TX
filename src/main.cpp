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

// ---- Reszta Twoich ustawień ----
#define PCF_ADDR 0x20
#define ADS_ADDR 0x48
#define OLED_ADDR 0x3C

Adafruit_PCF8574 pcf;
Adafruit_ADS1115 ads;
Adafruit_SH1106G display(128, 64, &Wire, -1);

#define TRIM_STEP 5
#define TRIM_MAX  100
int trim[4] = {0, 0, 0, 0};

enum { CH_THROTTLE = 0, CH_YAW = 1, CH_ROLL = 2, CH_PITCH = 3 };

struct AxisCal { int min; int max; int center; };
AxisCal axis[4] = {
  {12065, 15314, 13689}, // P0
  {11431, 14685, 13058}, // P1
  {12042, 15571, 13806}, // P2
  {11762, 15004, 13383}  // P3
};

#define RC_MIN 1000
#define RC_MAX 2000
#define RC_CENTER 1500
#define DEADZONE_RC 75

// Timer dla OLED
unsigned long lastDisplayUpdate = 0;
const int displayInterval = 200;

// ---- DEBOUNCING DLA PRZYCISKÓW ----
#define DEBOUNCE_DELAY 50  // 50ms debounce
unsigned long lastDebounceTime[8] = {0};
bool lastButtonState[8] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
bool buttonState[8] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Opcjonalne logowanie
}

int mapAxis(int raw, AxisCal &c, bool invert = false) {
  raw = constrain(raw, c.min, c.max);
  long mapped = invert ? map(raw, c.min, c.max, RC_MAX, RC_MIN) : map(raw, c.min, c.max, RC_MIN, RC_MAX);
  if (abs(mapped - RC_CENTER) < DEADZONE_RC) mapped = RC_CENTER;
  return mapped;
}

void handleTrim(int button) {
  switch (button) {
    case 4: trim[CH_THROTTLE] += TRIM_STEP; break;
    case 5: trim[CH_THROTTLE] -= TRIM_STEP; break;
    case 6: trim[CH_YAW]      += TRIM_STEP; break;
    case 7: trim[CH_YAW]      -= TRIM_STEP; break;
    case 2: trim[CH_PITCH]    += TRIM_STEP; break;
    case 3: trim[CH_PITCH]    -= TRIM_STEP; break;
    case 0: trim[CH_ROLL]     += TRIM_STEP; break;
    case 1: trim[CH_ROLL]     -= TRIM_STEP; break;
  }
  for (int i = 0; i < 4; i++) trim[i] = constrain(trim[i], -TRIM_MAX, TRIM_MAX);
}

void checkButtons() {
  for (uint8_t i = 0; i < 8; i++) {
    bool reading = pcf.digitalRead(i);
    
    // Jeśli stan się zmienił, resetuj timer
    if (reading != lastButtonState[i]) {
      lastDebounceTime[i] = millis();
    }
    
    // Jeśli minął czas debounce i stan jest stabilny
    if ((millis() - lastDebounceTime[i]) > DEBOUNCE_DELAY) {
      // Jeśli stan przycisku faktycznie się zmienił
      if (reading != buttonState[i]) {
        buttonState[i] = reading;
        
        // Reaguj tylko na naciśnięcie (zmiana HIGH -> LOW)
        if (buttonState[i] == LOW) {
          handleTrim(i);
        }
      }
    }
    
    lastButtonState[i] = reading;
  }
}

void setup() {
  Serial.begin(115200);
  
  // 1. Inicjalizacja WiFi i ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(onDataSent);
  
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // 2. Inicjalizacja I2C i peryferiów
  Wire.begin(21, 22);
  Wire.setClock(400000);

  if (!pcf.begin(PCF_ADDR, &Wire)) Serial.println("❌ PCF8574 error");
  for (uint8_t i = 0; i < 8; i++) pcf.pinMode(i, INPUT_PULLUP);

  if (!ads.begin(ADS_ADDR, &Wire)) Serial.println("❌ ADS1115 error");
  ads.setGain(GAIN_ONE);

  if (!display.begin(OLED_ADDR, true)) Serial.println("❌ OLED error");
  display.clearDisplay();
  display.display();
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  
  display.setCursor(32, 0);
  display.print("- RC SENDING -");
  display.drawFastHLine(0, 10, 128, SH110X_WHITE);

  display.setCursor(0, 18);
  display.print("THR: "); display.print(myData.throttle);
  display.setCursor(0, 28);
  display.print("YAW: "); display.print(myData.yaw);

  display.setCursor(68, 18);
  display.print("PIT: "); display.print(myData.pitch);
  display.setCursor(68, 28);
  display.print("ROL: "); display.print(myData.roll);

  display.drawFastHLine(0, 42, 128, SH110X_WHITE);
  display.setCursor(0, 48);
  display.print("TRIMS: ");
  display.setCursor(0, 57);
  display.printf("T:%d Y:%d P:%d R:%d", trim[CH_THROTTLE], trim[CH_YAW], trim[CH_PITCH], trim[CH_ROLL]);
  
  display.display();
}

void loop() {
  // --- ODCZYT I MAPOWANIE ---
  int16_t raw0 = ads.readADC_SingleEnded(0);
  int16_t raw1 = ads.readADC_SingleEnded(1);
  int16_t raw2 = ads.readADC_SingleEnded(2);
  int16_t raw3 = ads.readADC_SingleEnded(3);

  myData.throttle = constrain(mapAxis(raw0, axis[0], false) + trim[CH_THROTTLE], RC_MIN, RC_MAX);
  myData.yaw      = constrain(mapAxis(raw1, axis[1], true ) + trim[CH_YAW],      RC_MIN, RC_MAX);
  myData.roll     = constrain(mapAxis(raw2, axis[2], true ) + trim[CH_ROLL],     RC_MIN, RC_MAX);
  myData.pitch    = constrain(mapAxis(raw3, axis[3], true ) + trim[CH_PITCH],    RC_MIN, RC_MAX);

  // --- WYSYŁANIE ESP-NOW ---
  esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));

  // --- OBSŁUGA PRZYCISKÓW Z DEBOUNCINGIEM ---
  checkButtons();

  // --- AKTUALIZACJA WYŚWIETLACZA (Nieblokująca) ---
  if (millis() - lastDisplayUpdate > displayInterval) {
    updateOLED();
    lastDisplayUpdate = millis();
  }
}