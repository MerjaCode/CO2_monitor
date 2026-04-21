// I2C scanner, CUT MEAS bredge

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("\n--- SparkFun IoT RedBoard I2C Scanner ---");
  Serial.println("file = SPKFuN_IoT_RedBoard_I2C_Scan_Cut_MEAS");

  // 1. PIN 5 IS THE KEY: We must hold the power gate OPEN
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH); 
  delay(1000); // Give the sensors a full second to wake up

  Wire.begin();
  Wire.setClock(100000); // Standard speed for stability
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Scanning...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);

      // Mapping addresses to your hardware
      if (address == 0x28) Serial.println(" (XENSIV CO2 Sensor)");
      else if (address == 0x32) Serial.println(" (RV-8803 RTC)");
      else if (address == 0x36) Serial.println(" (MAX17048 Fuel Gauge)");
      else if (address == 0x3C) Serial.println(" (OLED Display)");
      else Serial.println(" (Unknown device)");

      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  if (nDevices == 0) Serial.println("No I2C devices found. Check Pin 5 Power Gate!\n");
  else Serial.println("Scan complete.\n");

  delay(5000); 
}
