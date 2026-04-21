#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <pas-co2-ino.hpp>
#include <SparkFun_RV8803.h> 
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>

// --- SETTINGS ---
#define SD_CS 5            // Shared with Power Gate
#define OLED_ADDR 0x3C
#define I2C_FREQ 100000
#define FAST_INTERVAL 10000 
#define SLOW_INTERVAL 600000 
#define THRESHOLD_MS 600000 

// --- OBJECTS ---
Adafruit_SSD1306 display(128, 64, &Wire, -1);
PASCO2Ino co2Sensor;
RV8803 rtc;
SFE_MAX1704X lipo(MAX1704X_MAX17048);

// --- GLOBALS ---
const char* fileName = "/co2_log.csv";
bool sdAvailable = false;
int16_t co2Level;
unsigned long lastMeasureTime = 0;

void logToSD(int16_t val, float v, float soc) {
  if (!sdAvailable) return;

  File dataFile = SD.open(fileName, FILE_APPEND);
  if (dataFile) {
    if (rtc.updateTime()) {
      // Create the concatenated timestamp string manually since stringTime() is a char*
      char fullTime[30];
      sprintf(fullTime, "%04d-%02d-%02d %s", 
              rtc.getYear() + 2000, rtc.getMonth(), rtc.getDate(), rtc.stringTime());
      
      // yyyy,mm,dd,hh,mm,ss,Timestamp,Millis,CO2,Volts,SOC%
      dataFile.printf("%04d,%02d,%02d,%02d,%02d,%02d,%s,%lu,%d,%.2f,%.1f\n",
                      rtc.getYear() + 2000, rtc.getMonth(), rtc.getDate(),
                      rtc.getHours(), rtc.getMinutes(), rtc.getSeconds(),
                      fullTime, millis(), val, v, soc);
    } else {
      dataFile.printf("ERR,ERR,ERR,ERR,ERR,ERR,RTC_FAIL,%lu,%d,%.2f,%.1f\n", millis(), val, v, soc);
    }
    dataFile.close();
    Serial.println("Logged to SD.");
  }
  digitalWrite(SD_CS, HIGH); // Power recovery for Qwiic rail

  // Recovery: Force Pin 5 back HIGH to ensure I2C power remains stable
  digitalWrite(SD_CS, HIGH);
}

void setup() {
  Serial.begin(115200);
  Serial.println("file = SPKFuN_IoT_RedBoard_I2C_SD_CO2_Bat_RTC.ino");
  
  // 1. FORCE POWER GATES HIGH (Essential for RedBoard IoT)
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH); 
  delay(1000); 

  Wire.begin();
  Wire.setClock(I2C_FREQ);

  // 2. INIT OLED
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.println("System Starting...");
    display.display();
  }

  // 3. INIT I2C DEVICES
  if (rtc.begin()) Serial.println("RTC Online.");
  if (lipo.begin()) Serial.println("Fuel Gauge Online.");
  if (co2Sensor.begin() == XENSIV_PASCO2_OK) {
    co2Sensor.startMeasure(FAST_INTERVAL / 1000);
  }

  // 4. INIT SD CARD
  SPI.begin(18, 19, 23);
  if (SD.begin(SD_CS)) {
    sdAvailable = true;
    if (!SD.exists(fileName)) {
      File dataFile = SD.open(fileName, FILE_WRITE);
      dataFile.println("yyyy,mm,dd,hh,mm,ss,Timestamp,Millis,CO2_ppm,Batt_V,SOC_pct");
      dataFile.close();
    }
    digitalWrite(SD_CS, HIGH); // Power Recovery
  }
}

void loop() {
  unsigned long currentMillis = millis();
  unsigned long interval = (currentMillis < THRESHOLD_MS) ? FAST_INTERVAL : SLOW_INTERVAL;

  if (currentMillis - lastMeasureTime >= interval) {
    lastMeasureTime = currentMillis;

    float v = lipo.getVoltage();
    float soc = lipo.getSOC();
    
    if (co2Sensor.getCO2(co2Level) == XENSIV_PASCO2_OK) {
      // Log to SD
      logToSD(co2Level, v, soc);

      // Update Serial
     // Update Serial - Removed .c_str() because stringTime() is already a char*
      Serial.printf("[%s] CO2: %d | Bat: %.2fV\n", rtc.stringTime(), co2Level, v);

      // Update OLED
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      if (rtc.updateTime()) display.print(rtc.stringTime());
      display.setCursor(90, 0);
      display.print(sdAvailable ? "[SD]" : "[!]");

      display.setCursor(0, 15);
      display.print("CO2 Concentration:");
      display.setTextSize(4);
      display.setCursor(10, 28);
      display.println(co2Level);

      display.setTextSize(1);
      display.setCursor(0, 56);
      display.printf("%.2fV (%.0f%%)  %s", v, soc, (interval == FAST_INTERVAL ? "FAST" : "LONG"));
      display.display();
    }
  }
  yield();
}
