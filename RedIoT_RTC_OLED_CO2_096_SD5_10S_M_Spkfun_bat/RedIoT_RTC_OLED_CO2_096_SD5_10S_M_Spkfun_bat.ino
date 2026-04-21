#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <pas-co2-ino.hpp>
#include <SparkFun_RV8803.h>
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>

// --- HARDWARE CONFIG ---
#define SD_CS          5     // SparkFun IoT RedBoard built-in slot
#define POWER_GATE     5     // CONFLICT PIN: Also enables peripheral power
#define OLED_ADDR      0x3C
#define FUEL_GAUGE_ADDR 0x36

// --- OBJECTS ---
Adafruit_SSD1306 display(128, 64, &Wire, -1);
PASCO2Ino co2Sensor;
RV8803 rtc;
SFE_MAX1704X lipo(MAX1704X_MAX17048);

// --- GLOBAL VARIABLES ---
int16_t co2Level;
bool sdAvailable = false;
const char* fileName = "/co2_log.csv";
unsigned long lastMeasureTime = 0;
#define FAST_INTERVAL 10000 

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // 1. STABILIZE POWER GATES
  // Since GPIO 5 is both SD_CS and Power Enable, we must pull it HIGH
  pinMode(POWER_GATE, OUTPUT);
  digitalWrite(POWER_GATE, HIGH); 
  delay(500);

  Wire.begin();
  Wire.setClock(100000);

  // 2. INITIALIZE PERIPHERALS
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.println("Booting...");
    display.display();
  }

  if (lipo.begin()) Serial.println("Fuel Gauge OK.");
  if (rtc.begin())  Serial.println("RTC OK.");
  
  if (co2Sensor.begin() == XENSIV_PASCO2_OK) {
    co2Sensor.startMeasure(FAST_INTERVAL / 1000);
    Serial.println("CO2 OK.");
  }

  // 3. SD SETUP (The "Careful" Way)
  if (SD.begin(SD_CS)) {
    sdAvailable = true;
    if (!SD.exists(fileName)) {
      File f = SD.open(fileName, FILE_WRITE);
      f.println("Date,Time,CO2_ppm,Batt_V,SOC_pct");
      f.close();
    }
    Serial.println("SD OK.");
  }
}

void loop() {
  if (millis() - lastMeasureTime >= FAST_INTERVAL) {
    lastMeasureTime = millis();

    // GET DATA
    float battV = lipo.getVoltage();
    float soc   = lipo.getSOC();
    rtc.updateTime();
    
    if (co2Sensor.getCO2(co2Level) == XENSIV_PASCO2_OK) {
      
      // LOG TO SD (This briefly toggles Pin 5)
      if (sdAvailable) {
        File dataFile = SD.open(fileName, FILE_APPEND);
        if (dataFile) {
          dataFile.printf("%s,%s,%d,%.2f,%.1f\n", 
                          rtc.stringDateUSA(), rtc.stringTime(), 
                          co2Level, battV, soc);
          dataFile.close();
        }
      }

      // UPDATE SERIAL
      Serial.printf("[%s] CO2: %d | Bat: %.2fV (%.1f%%)\n", 
                    rtc.stringTime(), co2Level, battV, soc);

      // UPDATE OLED
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0,0);
      display.print(rtc.stringTime());
      display.setCursor(90,0);
      display.print(sdAvailable ? "SD" : "!!");

      display.setCursor(0, 15);
      display.print("CO2 Level (PPM):");
      display.setTextSize(4);
      display.setCursor(10, 28);
      display.println(co2Level);

      display.setTextSize(1);
      display.setCursor(0, 56);
      display.printf("%.2fV (%.0f%%)", battV, soc);
      display.display();
    }
  }
}
