#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <pas-co2-ino.hpp>
#include <SparkFun_RV1805.h> 
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- HARDWARE MAPPING ---
#define SD_CS 5            // Power Gate & SD Chip Select
#define WAKE_BUTTON 4      // Push button to GND for manual trigger
#define SLEEP_MINUTES 10    
#define BURST_DURATION 30   

// --- OBJECTS ---
RV1805 rtc; 
PASCO2Ino co2Sensor;
SFE_MAX1704X lipo(MAX1704X_MAX17048);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

const char* fileName = "/co2_log.csv";

void runCaptureCycle() {
  // 1. POWER UP
  digitalWrite(SD_CS, HIGH); 
  delay(500);
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  rtc.begin();
  lipo.begin();
  co2Sensor.begin();
  co2Sensor.startMeasure();

  // 2. TRANSIENT BURST (The "Capture")
  SPI.begin(18, 19, 23);
  bool sdReady = SD.begin(SD_CS);

  for (int i = 0; i < BURST_DURATION; i++) {
    rtc.updateTime();
    int16_t currentCO2 = 0;
    co2Sensor.getCO2(currentCO2);
    float v = lipo.getVoltage();
    float soc = lipo.getSOC();

    // Log to SD
    if (sdReady) {
      File d = SD.open(fileName, FILE_APPEND);
      if (d) {
        // yyyy-mm-dd hh:mm:ss, BurstSec, CO2, Volts, SOC
        d.printf("%04d-%02d-%02d %s,%d,%d,%.2f,%.1f\n",
                 rtc.getYear()+2000, rtc.getMonth(), rtc.getDate(),
                 rtc.stringTime(), i, currentCO2, v, soc);
        d.close();
      }
    }

    // Update OLED (Visual Feedback for "Force Wake")
    display.clearDisplay();
    display.setCursor(0,0);
    display.printf("BURST: %ds/%ds", i, BURST_DURATION);
    display.setCursor(0,15);
    display.printf("CO2: %d PPM", currentCO2);
    display.setCursor(0,55);
    display.printf("%.2fV (%.0f%%)", v, soc);
    display.display();

    digitalWrite(SD_CS, HIGH); // Re-assert gate
    delay(1000); 
  }

  // 3. SHUTDOWN
  display.clearDisplay();
  display.setCursor(0,20);
  display.print("SLEEPING...");
  display.display();
  delay(1000);
  digitalWrite(SD_CS, LOW); // Kill Qwiic Rail
}

void setup() {
  Serial.begin(9600);
  Serial.println("file = SparkRedIoT_Wake_record.ino");
  pinMode(SD_CS, OUTPUT);
  
  // Setup Force Wake Button (Active Low)
  pinMode(WAKE_BUTTON, INPUT_PULLUP);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_BUTTON, 0); // 0 = Wake on Low
  
  // Setup Timer Wake
  esp_sleep_enable_timer_wakeup(SLEEP_MINUTES * 60ULL * 1000000ULL);

  // Run the cycle immediately on wake
  runCaptureCycle();

  // Go to Sleep
  esp_deep_sleep_start();
}

void loop() {}
