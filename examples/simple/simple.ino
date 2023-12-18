#include <Arduino.h>  // Some Arduino cores use the arduino namespace. This is addressed in Arduino.h.
#include <Wire.h>    // TwoWire

#include <TMP1075.h>    // Use quotation marks if the library is in the same folder as the sketch
// #include "TMP1075/src/TMP1075.h"    // Use this import, if you copied the library folder into the sketch folder

//TwoWire wire = TwoWire(1, I2C_REMAP, 400*1000);     // Arduino_STM32 use I²C 1 with alternative pin mapping
TwoWire wire = Wire;
TMP1075::TMP1075 tmp1075 = TMP1075::TMP1075(wire);    // The library uses the namespace TMP1075

void setup() {
    Serial.begin(115200);
    wire.begin(0x48);  // See definition of wire above
    tmp1075.begin();  // Syncs the config register
}

void loop() {
    // The Teensy and STM32 will lock up, if the input buffer overflows
    while (Serial.available()) {
        Serial.read();
    }
    static unsigned long lastTime = millis();    // Initialize lastTime *once* during the first loop iteration
    if (millis() - lastTime > 1000) {
        lastTime = millis();
        tmp1075.setConversionTime(TMP1075::ConversionTime220ms);
        Serial.print(tmp1075.getTemperatureCelsius());
        Serial.print("\n");
    }
}