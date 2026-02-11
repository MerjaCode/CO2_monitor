/*
  XENSIV PAS CO2 Logger with OLED and SD Card
  Hardware: 
  - SparkFun IoT ESP32 RedBoard
  - XENSIV PAS CO2 Sensor (I2C)
  - 0.96" SSD1306 OLED (I2C)
  - Built-in microSD slot (Auto-detecting Chip Select: GPIO 10 or 5)

  STATUS: reboots fixed (DIO mode). 
  FIXING: SD Detection. Trying auto-switch between Pin 10 and Pin 5.
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
const char* fileName = "/co2_log.csv";
bool sdAvailable = false; 
int activeCS = 10; // We will try 10 first, then fallback to 5

// CO2 Sensor Configuration
#define I2C_FREQ_HZ 100000          
#define MEASURE_INTERVAL_MS 10000   
#define PRESSURE_REFERENCE 900

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
    
    // We try at 4MHz for stability
    if (SD.begin(pin, SPI, 4000000)) {
        return true;
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    Serial.println("file = OLED_CO2_RedIoT_096_SD5_10sec_wrk");
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

    // 2. Initialize CO2 sensor (Confirmed Working)
    Serial.println("Initializing CO2 Sensor...");
    err = co2Sensor.begin();
    if (XENSIV_PASCO2_OK != err) {
        display.println("Sensor: ERROR");
    } else {
        co2Sensor.setPressRef(PRESSURE_REFERENCE);
        co2Sensor.startMeasure(MEASURE_INTERVAL_MS / 1000);
        display.println("Sensor: OK");
    }
    display.display();

    // 3. Initialize SD Card
    if (useSD) {
        Serial.println("Starting SPI Bus (18, 19, 23)...");
        SPI.begin(18, 19, 23); 
        yield();
        delay(500);

        // Try Pin 10 First
        if (trySDInit(10)) {
            activeCS = 10;
            sdAvailable = true;
            Serial.println("SD Card found on GPIO 10!");
        } 
        // Try Pin 5 as Fallback
        else if (trySDInit(5)) {
            activeCS = 5;
            sdAvailable = true;
            Serial.println("SD Card found on GPIO 5!");
        }

        if (!sdAvailable) {
            Serial.println("SD Card NOT found on Pin 10 or Pin 5.");
            display.println("SD Card: FAIL");
        } else {
            // Check card type to confirm communication
            uint8_t cardType = SD.cardType();
            if(cardType == CARD_NONE){
                Serial.println("Card type: NONE. Check insertion.");
                sdAvailable = false;
                display.println("SD: No Card");
            } else {
                Serial.println("SD Card ready and mounted.");
                display.println("SD Card: OK");
                
                // Create Header if needed
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

    if (currentMillis - lastMeasureTime >= MEASURE_INTERVAL_MS) {
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
            Serial.print("CO2: "); Serial.print(co2Level); Serial.println(" ppm");
            
            if (sdAvailable) logToSD(co2Level);
            
            display.print("CO2 Level (ppm):");
            display.setTextSize(4);
            display.setCursor(10, 30);
            display.println(co2Level);
            
            display.setTextSize(1);
            display.setCursor(80, 0);
            display.printf(sdAvailable ? "[SD:%d]" : "[NO SD]", activeCS);
        } else {
            display.println("Sensor Error");
            display.print("Code: ");
            display.println(err);
        }
        
        display.display();
        co2Sensor.setPressRef(PRESSURE_REFERENCE);
    }
    
    yield(); 
}