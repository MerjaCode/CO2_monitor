#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <pas-co2-ino.hpp>
#include <SparkFun_RV1805.h> 
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SD_CS 5            
#define BURST_DURATION 45

void runCaptureCycle();

RV1805 rtc; 
PASCO2Ino co2Sensor;
SFE_MAX1704X lipo(MAX1704X_MAX17048);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

void setup() {
  Serial.begin(9600);
  Serial.println("\n--- BOOT ---");
  Serial.println("file = SPKFuN_RV1805_Neutral_v44.ino");
  
  // DIAGNOSTIC
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if(cause == ESP_SLEEP_WAKEUP_TIMER) Serial.println("Wake Reason: Timer");
  else Serial.println("Wake Reason: Power/Reset/Crash");

  // TIMER ONLY: 10 Minutes
  esp_sleep_enable_timer_wakeup(600ULL * 1000000ULL);

  runCaptureCycle();
  
  Serial.println("System Neutral. Entering Sleep in 5s...");
  delay(5000); 
  esp_deep_sleep_start();
}

void runCaptureCycle() {
  // POWER ON
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH); 
  delay(3000); 

  Wire.begin();
  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.display();
  }

  rtc.begin();
  lipo.begin();
  if(co2Sensor.begin() == XENSIV_PASCO2_OK) co2Sensor.startMeasure(5); 

  SPI.begin(18, 19, 23);
  bool sdReady = SD.begin(SD_CS);

  int16_t lastGoodCO2 = 0;
  for (int i = 0; i < BURST_DURATION; i++) {
    rtc.updateTime();
    int16_t currentCO2 = 0;
    co2Sensor.getCO2(currentCO2);
    
    if (currentCO2 > 0) {
      lastGoodCO2 = currentCO2;
      if (sdReady) {
        File d = SD.open("/co2_log.csv", FILE_APPEND);
        if (d) {
          d.printf("%s,%d,%d,%.2f,%.1f\n", rtc.stringTime(), i, currentCO2, lipo.getVoltage(), lipo.getSOC());
          d.close();
        }
      }
    }

    display.clearDisplay();
    display.setCursor(0,0);
    display.printf("T: %s", rtc.stringTime()); 
    display.setCursor(0,15);
    display.setTextSize(3);
    if(lastGoodCO2 == 0) display.print("WAIT");
    else display.print(lastGoodCO2);
    display.setTextSize(1);
    display.setCursor(0,45);
    display.printf("%.2fV (%.0f%%)", lipo.getVoltage(), lipo.getSOC());
    display.setCursor(0,55);
    display.printf("TICK: %d/%d", i+1, BURST_DURATION);
    display.display();
    delay(1000); 
  }
  
  // NO SHUTDOWN COMMANDS: Keep everything alive to prevent transition spike
  Serial.println("Burst Complete.");
}

void loop() {}