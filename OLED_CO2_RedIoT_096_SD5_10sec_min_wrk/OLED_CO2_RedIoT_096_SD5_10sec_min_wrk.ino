/*
  XENSIV PAS CO2 Logger with OLED and SD Card
  Hardware: 
  - SparkFun IoT ESP32 RedBoard
  - XENSIV PAS CO2 Sensor (I2C)
  - 0.96" SSD1306 OLED (I2C)
  - Built-in microSD slot (Auto-detecting Chip Select: GPIO 10 or 5)

  STATUS: SD card fixed (DIO mode + pin detection).
  DYNAMICS: 10s intervals for first 5 mins, then 10min intervals.
*/

#include <Arduino.h>
#include <pas-co2-ino.hpp>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>

// --- USER SETTINGS ---
bool useSD = true;          
// ---------------------

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// SD Configuration
const char* fileName = "/co2a_log.csv";
bool sdAvailable = false; 
int activeCS = 10; 

// CO2 Sensor Configuration
#define I2C_FREQ_HZ 100000          
#define PRESSURE_REFERENCE 900

// Dynamic Timing Constants
#define FAST_INTERVAL_MS 10000    // 10 seconds
#define SLOW_INTERVAL_MS 600000   // 10 minutes (600,000 ms)
#define THRESHOLD_MS 300000       // 5 minutes (300,000 ms)

PASCO2Ino co2Sensor;
int16_t co2Level;
Error_t err;
unsigned long lastMeasureTime = 0;

void logToSD(int16_t val) {
  if (!sdAvailable) return; 

  File dataFile = SD.open(fileName, FILE_APPEND);
  if (dataFile) {
    dataFile.print(millis());
    dataFile.print(",");
    dataFile.println(val);
    dataFile.close();
    Serial.println("Data appended to SD.");
  } else {
    Serial.println("SD Write Error!");
  }
}

bool trySDInit(int pin) {
    Serial.printf("Attempting SD.begin on GPIO %d...\n", pin);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    yield();
    
    if (SD.begin(pin, SPI, 4000000)) {
        return true;
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    Serial.println("file = OLED_CO2_RedIoT_096_SD5_10sec_min_wrk");
    delay(2000); 
    Serial.println("\n--- Master Grain Bin System Boot ---");
    
    Wire.begin();
    Wire.setClock(I2C_FREQ_HZ);
    yield();

    // 1. Initialize OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("OLED failed."));
    } else {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.println("System Starting...");
        display.display();
    }

    // 2. Initialize CO2 sensor
    Serial.println("Initializing CO2 Sensor...");
    err = co2Sensor.begin();
    if (XENSIV_PASCO2_OK != err) {
        display.println("Sensor: ERROR");
    } else {
        co2Sensor.setPressRef(PRESSURE_REFERENCE);
        // Start with fast measurement; we will handle intervals in loop
        co2Sensor.startMeasure(FAST_INTERVAL_MS / 1000);
        display.println("Sensor: OK");
    }
    display.display();

    // 3. Initialize SD Card
    if (useSD) {
        Serial.println("Starting SPI Bus (18, 19, 23)...");
        SPI.begin(18, 19, 23); 
        yield();
        delay(500);

        if (trySDInit(10)) {
            activeCS = 10;
            sdAvailable = true;
            Serial.println("SD Card found on GPIO 10!");
        } 
        else if (trySDInit(5)) {
            activeCS = 5;
            sdAvailable = true;
            Serial.println("SD Card found on GPIO 5!");
        }

        if (!sdAvailable) {
            Serial.println("SD Card NOT found on Pin 10 or Pin 5.");
            display.println("SD Card: FAIL");
        } else {
            uint8_t cardType = SD.cardType();
            if(cardType == CARD_NONE){
                sdAvailable = false;
                display.println("SD: No Card");
            } else {
                display.println("SD Card: OK");
                if (!SD.exists(fileName)) {
                    File dataFile = SD.open(fileName, FILE_WRITE);
                    if (dataFile) {
                        dataFile.println("Timestamp_ms,CO2_ppm");
                        dataFile.close();
                    }
                }
            }
        }
    } else {
        display.println("SD Card: DISABLED");
    }
    
    display.display();
    Serial.println("Setup Complete. Loop started.");
    delay(2000);
}

void loop() {
    unsigned long currentMillis = millis();
    
    // Determine the current required interval
    // If uptime is less than 5 minutes, use 10 seconds. Otherwise, use 10 minutes.
    unsigned long currentInterval = (currentMillis < THRESHOLD_MS) ? FAST_INTERVAL_MS : SLOW_INTERVAL_MS;

    // Check if it's time to take a measurement
    // This subtraction is rollover-safe for ~49 days
    if (currentMillis - lastMeasureTime >= currentInterval) {
        lastMeasureTime = currentMillis;

        err = co2Sensor.getCO2(co2Level);
        
        if (XENSIV_PASCO2_ERR_COMM == err) {
            delay(600);
            err = co2Sensor.getCO2(co2Level);
        }
        
        display.clearDisplay();
        display.setCursor(0, 0);
        display.setTextSize(1);
        
        if (XENSIV_PASCO2_OK == err) {
            Serial.printf("CO2: %d ppm [Interval: %lus]\n", co2Level, currentInterval / 1000);
            
            if (sdAvailable) logToSD(co2Level);
            
            display.print("CO2 Level (ppm):");
            display.setTextSize(4);
            display.setCursor(10, 30);
            display.println(co2Level);
            
            display.setTextSize(1);
            display.setCursor(80, 0);
            display.printf(sdAvailable ? "[SD:%d]" : "[NO SD]", activeCS);
            
            // Show a countdown/status for interval mode
            display.setCursor(0, 56);
            if (currentInterval == FAST_INTERVAL_MS) {
              display.print("Mode: Initial (10s)");
            } else {
              display.print("Mode: Long-term (10m)");
            }
        } else {
            display.println("Sensor Error");
            display.print("Code: ");
            display.println(err);
        }
        
        display.display();
        
        // Update pressure reference and adjust sensor internal interval if needed
        co2Sensor.setPressRef(PRESSURE_REFERENCE);
    }
    
    yield(); 
}