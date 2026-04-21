#include <Wire.h>
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h> // Library for the MAX17048
#include <SparkFun_RV8803.h>                              // Library for the RTC
#include <pas-co2-ino.hpp>                                // Library for the CO2

SFE_MAX1704X lipo(MAX1704X_MAX17048);
RV8803 rtc;
PASCO2Ino co2Sensor;

void setup() {
    Serial.begin(115200);
    Wire.begin();
    delay(1000); 

    Serial.println("\n--- SPARKFUN REDBOARD DIAGNOSTIC ---");

    // 1. Initialize Battery Fuel Gauge (The Real Way)
    if (lipo.begin()) {
        Serial.println("Battery Fuel Gauge: ONLINE (0x36)");
    } else {
        Serial.println("Battery Fuel Gauge: NOT FOUND. Check I2C bus.");
    }

    // 2. Initialize RTC
    if (rtc.begin()) {
        Serial.println("RTC: ONLINE (0x32)");
    } else {
        Serial.println("RTC: NOT FOUND.");
    }

    // 3. Initialize CO2
    if (co2Sensor.begin() == XENSIV_PASCO2_OK) {
        Serial.println("CO2: ONLINE (0x28)");
        co2Sensor.startMeasure();
    }
}

void loop() {
    // Battery Data from the MAX17048
    float volts = lipo.getVoltage();
    float soc = lipo.getSOC();

    // Time from RTC
    rtc.updateTime();

    // CO2 Data
    int16_t co2Val;
    co2Sensor.getCO2(co2Val);

    Serial.printf("[%s] Bat: %.2fV (%.1f%%) | CO2: %d ppm\n", 
                  rtc.stringTime(), volts, soc, co2Val);

    delay(5000);
}