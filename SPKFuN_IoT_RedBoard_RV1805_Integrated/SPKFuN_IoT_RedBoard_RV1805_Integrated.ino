#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <pas-co2-ino.hpp>
#include <SparkFun_RV1805.h> // Correct Library for V-1805
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>

// --- HARDWARE & PIN DEFINITIONS ---
// GPIO 5 is the Gate for the Qwiic 3.3V rail and the SD Chip Select
#define SD_CS 5            
#define OLED_ADDR 0x3C     
#define FAST_INTERVAL 10000 
#define SLOW_INTERVAL 600000 
#define THRESHOLD_MS 600000 

// --- OBJECTS ---
Adafruit_SSD1306 display(128, 64, &Wire, -1);
PASCO2Ino co2Sensor;
RV1805 rtc; // Correct Object
SFE_MAX1704X lipo(MAX1704X_MAX17048);

// --- GLOBAL VARIABLES ---
const char* fileName = "/co2_log.csv";
bool sdAvailable = false;
int16_t co2Level = 0;
unsigned long lastMeasureTime = 0;
uint32_t logCount = 0;

/**
 * FUNCTION: logToSD
 * Writes data to the SD card.
 * NOTE: RV1805 uses getYear() differently; we handle the 2000 offset here.
 */
void logToSD(int16_t val, float v, float soc) {
  if (!sdAvailable) return;
  
  File dataFile = SD.open(fileName, FILE_APPEND);
  if (dataFile) {
    // Correctly handle the char* return type
    char* timeStr = rtc.stringTime(); 
    int fullYear = rtc.getYear() + 2000;
    
    dataFile.printf("%04d,%02d,%02d,%02d,%02d,%02d,%s,%lu,%d,%.2f,%.1f\n",
                    fullYear, rtc.getMonth(), rtc.getDate(),
                    rtc.getHours(), rtc.getMinutes(), rtc.getSeconds(),
                    timeStr, millis(), val, v, soc); // Removed .c_str()
    dataFile.close();
    logCount++;
  }
  digitalWrite(SD_CS, HIGH); 
}
void setup() {
  Serial.begin(9600); // Set to 9600 to match your working RV-1805 sketch
  Serial.println("\nfile = SPKFuN_IoT_RedBoard_RV1805_Integrated.ino");
  
  // 1. POWER GATE: Wake up the I2C sensors
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH); 
  delay(1500); 

  Wire.begin();
  
  // 2. OLED START
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println("RV-1805 ONLINE");
    display.display();
  }

  // 3. RTC START
  if (rtc.begin() == false) {
    Serial.println("RV-1805 not found! Check wiring.");
  }
  // Sync to compiler time (ensure your IDE is fresh for best results)
  rtc.setToCompilerTime();

  // 4. SENSOR INIT
  lipo.begin();
  co2Sensor.begin();
  co2Sensor.startMeasure(FAST_INTERVAL / 1000);

  // 5. SD INIT & HEADER
  SPI.begin(18, 19, 23);
  if (SD.begin(SD_CS)) {
    sdAvailable = true;
    File f = SD.open(fileName, FILE_APPEND);
    if (f) {
      f.println("\n--- RV-1805 LOGGING SESSION START ---");
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

    // A. Update Time variables from hardware
    if (rtc.updateTime() == false) {
      Serial.println("RTC failed to update!");
    }

    // B. Gather Battery Data
    float v = lipo.getVoltage();
    float soc = lipo.getSOC();

    // C. Gather CO2 (with 3-retry safety)
    int retry = 0;
    while(retry < 3) {
      if (co2Sensor.getCO2(co2Level) == XENSIV_PASCO2_OK && co2Level > 0) break;
      delay(500); 
      retry++;
    }

    // D. Write to SD Card
    logToSD(co2Level, v, soc);

    // E. OLED Refresh
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(rtc.stringTime()); // RV1805 library returns a String object
    display.setCursor(95, 0);
    display.print(sdAvailable ? "SD" : "!!");
    
    display.setCursor(0, 15);
    display.print("CO2 (PPM):");
    display.setTextSize(4);
    display.setCursor(10, 28);
    display.println(co2Level);

    display.setTextSize(1);
    display.setCursor(0, 56);
    display.printf("%.2fV (%.0f%%)  #:%u", v, soc, logCount);
    display.display();

    Serial.printf("[%s] CO2: %d | Bat: %.2fV\n", rtc.stringTime(), co2Level, v);
  }
}
