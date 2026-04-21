#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <pas-co2-ino.hpp>
#include <SparkFun_RV8803.h> 
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>

#define SD_CS 5            
#define OLED_ADDR 0x3C
#define FAST_INTERVAL 10000 
#define SLOW_INTERVAL 600000 
#define THRESHOLD_MS 600000 

Adafruit_SSD1306 display(128, 64, &Wire, -1);
PASCO2Ino co2Sensor;
RV8803 rtc;
SFE_MAX1704X lipo(MAX1704X_MAX17048);

const char* fileName = "/co2_log.csv";
bool sdAvailable = false;
int16_t co2Level = 0;
unsigned long lastMeasureTime = 0;
uint32_t logCount = 0; // Tracks total successful SD writes

void logToSD(int16_t val, float v, float soc) {
  if (!sdAvailable) return;
  
  File dataFile = SD.open(fileName, FILE_APPEND);
  if (dataFile) {
    char fullTime[32];
    sprintf(fullTime, "%04d-%02d-%02d %s", 
            rtc.getYear() + 2000, rtc.getMonth(), rtc.getDate(), rtc.stringTime());
    
    // Pluralized time functions for the spreadsheet columns
    dataFile.printf("%04d,%02d,%02d,%02d,%02d,%02d,%s,%lu,%d,%.2f,%.1f\n",
                    rtc.getYear() + 2000, rtc.getMonth(), rtc.getDate(),
                    rtc.getHours(), rtc.getMinutes(), rtc.getSeconds(),
                    fullTime, millis(), val, v, soc);
    dataFile.close();
    logCount++; // Increment counter on success
    Serial.println("Data Logged.");
  }
  digitalWrite(SD_CS, HIGH); // Restore power to Qwiic rail
}

void setup() {
  Serial.begin(115200);
  Serial.println("file = SPKFuN_IoT_RedBoard_I2C_SD_CO2_Bat_RTCc.ino");
  
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH); 
  delay(1500); 

  Wire.begin();
  Wire.setClock(100000);

  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println("Syncing Sensors...");
    display.display();
  }

  if (rtc.begin()) {
    rtc.disableAllInterrupts(); 
    rtc.clearAllInterruptFlags();
    rtc.setToCompilerTime();    
    Serial.println("RTC Resynced.");
  }

  lipo.begin();
  co2Sensor.begin();
  co2Sensor.startMeasure(FAST_INTERVAL / 1000);

  SPI.begin(18, 19, 23);
  if (SD.begin(SD_CS)) {
    sdAvailable = true;
    if (!SD.exists(fileName)) {
      File f = SD.open(fileName, FILE_WRITE);
      f.println("yyyy,mm,dd,hh,mm,ss,Timestamp,Millis,CO2_ppm,Batt_V,SOC_pct");
      f.close();
    }
  }
  digitalWrite(SD_CS, HIGH);
}

void loop() {
  unsigned long currentMillis = millis();
  unsigned long interval = (currentMillis < THRESHOLD_MS) ? FAST_INTERVAL : SLOW_INTERVAL;

  if (currentMillis - lastMeasureTime >= interval) {
    lastMeasureTime = currentMillis;

    rtc.updateTime();
    float v = lipo.getVoltage();
    float soc = lipo.getSOC();
    co2Sensor.getCO2(co2Level);

    logToSD(co2Level, v, soc);

    Serial.printf("[%s] CO2: %d | Bat: %.2fV | Logs: %u\n", rtc.stringTime(), co2Level, v, logCount);

    display.clearDisplay();
    
    // Header
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(rtc.stringTime());
    display.setCursor(95, 0);
    display.print(sdAvailable ? "SD" : "!!");

    // Middle
    display.setCursor(0, 15);
    display.print("CO2 (PPM):");
    display.setTextSize(4);
    display.setCursor(10, 28);
    display.println(co2Level);

    // Footer
    display.setTextSize(1);
    display.setCursor(0, 56);
    display.printf("%.2fV (%.0f%%)", v, soc);
    
    // Log Counter in bottom right
    display.setCursor(85, 56);
    display.printf("#:%u", logCount);
    
    display.display();
  }
}
