# CO2_monitor
CO2 monitor for rooms and grain bins.  Eventually upload to Google Sheets
OLED_CO2_RedIoT_096_SD5_10sec & ...min have...
  XENSIV PAS CO2 Logger with OLED and SD Card
  Hardware: 
  - SparkFun IoT ESP32 RedBoard
  - XENSIV PAS CO2 Sensor (I2C)
  - 0.96" SSD1306 OLED (I2C)
  - Built-in microSD slot (Auto-detecting Chip Select: GPIO 10 or 5)

  STATUS: reboots fixed (DIO mode). 
  FIXING: SD Detection. Trying auto-switch between Pin 10 and Pin 5.

  LIBRARIES NEEDED:

    #include <Arduino.h>
    #include <pas-co2-ino.hpp>
    #include <Wire.h>
    #include <Adafruit_GFX.h>
    #include <Adafruit_SSD1306.h>
    #include <SPI.h>
    #include <SD.h>
    -------------------------------

  OLED_CO2_RedIoT_096_SD5_10sec does 10 sec updates whereas OLED_CO2_RedIoT_096_SD5_10sec_min does 10 sec updates for the first ____ min, then 10 minute updates thereafter, using millis, so good for 49 ish days before rollover.

FUTURE....
  It would be good to add a RTC so we could date/time the data.
  Add bigger touch screen to select grain bin we are testing. or "other" environment... maybe where are we and allow input...ie bin number, CS house, Science room etc that would go into a location field.
  Enable ESP32 to search for SSID on list and tell us where we are connected. 
