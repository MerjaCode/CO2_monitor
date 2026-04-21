/*
  XENSIV PAS CO2 Logger with OLED, SD Card, and Battery Monitor
  Hardware: 
  - SparkFun IoT ESP32 RedBoard
  - XENSIV PAS CO2 Sensor (I2C)
  - 0.96" SSD1306 OLED (I2C)
  - SparkFun Qwiic RTC (RV-8803)
  - Built-in microSD slot (Dual-check CS: 5 then 10)
  
  STATUS: Always-On Mode with Real-Time Battery Sensing.
*/

#include <Arduino.h>
#include <pas-co2-ino.hpp>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>
#include <SparkFun_RV8803.h> // gemini had SparkFun_RV8803_Arduino_Library

// --- CONFIGURATION ---
bool useSD = true;
const char* fileName = "/co2_log.csv";
#define I2C_FREQ_HZ 100000
#define PRESSURE_REFERENCE 900
#define BATT_PIN 34           // Internal Battery Pin for RedBoard IoT
#define ADC_RESOLUTION 4095.0
#define V_REF 3.3

// Timing: 10s for first 10 mins, then 10m intervals
#define FAST_INTERVAL_MS 10000 
#define SLOW_INTERVAL_MS 600000 
#define THRESHOLD_MS 600000 

// --- OBJECTS ---
Adafruit_SSD1306 display(128, 64, &Wire, -1);
PASCO2Ino co2Sensor;
RV8803 rtc;
int16_t co2Level;
bool sdAvailable = false;
int activeCS = 10;
unsigned long lastMeasureTime = 0;

// --- FUNCTIONS ---

float getBatteryVoltage() {
  // Take 10 readings and average to smooth out ADC noise
  float sum = 0;
  for(int i=0; i<10; i++) {
    sum += analogRead(BATT_PIN);
    delay(1);
  }
  float avgRaw = sum / 10.0;
  // Divider ratio on RedBoard is 1:2, so multiply by 2.0
  float voltage = (avgRaw / ADC_RESOLUTION) * V_REF * 2.0;
  return voltage;
}

void logToSD(int16_t val, float battV) {
  if (!sdAvailable) return;
  File dataFile = SD.open(fileName, FILE_APPEND);
  if (dataFile) {
    if (rtc.updateTime()) {
      dataFile.print(rtc.stringDateUSA()); dataFile.print(" ");
      dataFile.print(rtc.stringTime());
    } else {
      dataFile.print("RTC_ERR");
    }
    dataFile.print(","); dataFile.print(millis());
    dataFile.print(","); dataFile.print(val);
    dataFile.print(","); dataFile.println(battV, 2);
    dataFile.close();
    Serial.println("Logged to SD.");
  }
}

bool trySDInit(int pin) {
  Serial.printf("Checking SD on GPIO %d...\n", pin);
  if (SD.begin(pin, SPI, 4000000)) return true;
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- OLED_CO2_RedIoT_096_SD5_10sec_min_BAT ---");

  Wire.begin();
  Wire.setClock(I2C_FREQ_HZ);

  // 1. OLED Init
  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println("System Boot...");
    display.display();
  }

  // 2. RTC Init
  if (rtc.begin()) {
    Serial.println("RTC Online.");
    // rtc.setToCompilerTime(); // Uncomment/Re-upload ONCE to sync time
  }

  // 3. CO2 Sensor Init
  if (co2Sensor.begin() == XENSIV_PASCO2_OK) {
    co2Sensor.setPressRef(PRESSURE_REFERENCE);
    co2Sensor.startMeasure(FAST_INTERVAL_MS / 1000);
    Serial.println("CO2 Sensor OK.");
  }

  // 4. SD Card Init (Safe Sequence)
  if (useSD) {
    SPI.begin(18, 19, 23);
    delay(100);
    if (trySDInit(5)) { 
      activeCS = 5; sdAvailable = true; 
    } else {
      delay(500);
      if (trySDInit(10)) { activeCS = 10; sdAvailable = true; }
    }

    if (sdAvailable) {
      if (!SD.exists(fileName)) {
        File dataFile = SD.open(fileName, FILE_WRITE);
        dataFile.println("Date,Time,Millis,CO2_ppm,Batt_V");
        dataFile.close();
      }
      Serial.println("SD Setup Successful.");
    }
  }

  display.clearDisplay();
  display.println("Setup Complete!");
  display.display();
  delay(1000);
}

void loop() {
  unsigned long currentMillis = millis();
  unsigned long currentInterval = (currentMillis < THRESHOLD_MS) ? FAST_INTERVAL_MS : SLOW_INTERVAL_MS;

  if (currentMillis - lastMeasureTime >= currentInterval) {
    lastMeasureTime = currentMillis;
    float battV = getBatteryVoltage();

    if (co2Sensor.getCO2(co2Level) == XENSIV_PASCO2_OK) {
      // 1. Log to SD
      logToSD(co2Level, battV);

      // 2. Print to Serial
      Serial.printf("[%lu] CO2: %d ppm | Batt: %.2fV\n", currentMillis, co2Level, battV);

      // 3. Update OLED
      display.clearDisplay();
      
      // Top Bar: RTC Time & SD Status
      display.setTextSize(1);
      display.setCursor(0,0);
      if(rtc.updateTime()) display.print(rtc.stringTime());
      display.setCursor(85, 0);
      display.print(sdAvailable ? "[SD]" : "[!]");

      // Middle: CO2 Value
      display.setCursor(0, 15);
      display.print("CO2 Concentration:");
      display.setTextSize(4);
      display.setCursor(10, 28);
      display.println(co2Level);

      // Bottom Bar: Battery Voltage
      display.setTextSize(1);
      display.setCursor(0, 56);
      display.print("Battery: ");
      display.print(battV, 2);
      display.print("V");
      
      // Mode Indicator
      display.setCursor(90, 56);
      display.print(currentInterval == FAST_INTERVAL_MS ? "FAST" : "LONG");

      display.display();
    }
  }
  yield();
}
