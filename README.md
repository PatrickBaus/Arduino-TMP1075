# Arduino TMP1075 Library
This is a C++ Arduino library for the [Texas Instruments TMP1075](https://www.ti.com/product/TMP1075) temperature sensor. It uses the I²C interface and is intended as an LM75 replacement.

## API
#### Constructor:
```c
  TMP1075(TwoWire &wire, uint8_t i2cAdress = 0x48);
```
___Arguments___
* `wire` [TwoWire] : The [Wire](https://www.arduino.cc/en/reference/wire) interface used to transmit the data. The interface can be configured before calling `begin()`, e.g. to support alternative pin configurations.
* `i2cAdress` [uint8_t] : The I²C address of the TMP1075 as configured using the A0, A1, A2 pins. The default is `0x48`, which is the address when all three pins are tied low.

### Methods
```c
  void begin();
```
This function calls the [`Wire.begin()`](https://www.arduino.cc/en/Reference/WireBegin) function interally, using the `i2cAdress` previously set.
```c
  uint8_t getDeviceId();
```
___Returns___
* [uint8_t] : The sensor device ID. This should be `0x75`, if everything is set up correctly.

```c
  int32_t getTemperatureRaw();
  float getTemperatureCelsius();
```
___Returns___
* [int16_t] : The raw temperature data as returned by the sensor. A 16-bit integer that represents the temperature range from –128 °C to 127.9375 °C in steps of 0.0625 K. The 4 LSB are always 0. To convert from raw values to Celsius one needs to calculate `value / 16 * 0.0625`. Note: Beware of bit shifts, because the return value is a signed integer.
* [float] : The temperature in units of °C.

```c
  void setConversionMode(const bool isSingleShot=false);
```
___Arguments___
* `isSingleShot` [bool] : Setting this to true, will put the sensor in a low power state, that requires a call to `startConversion()`to start a new conversion.

```c
  bool getConversionMode();
```
___Returns___
* [bool] : The conversion mode as described above.

```c
  void startConversion();
```
Immediately start a new conversion. The result will be available after the conversion time. See `setConversionTime()`.

```c
  void setConversionTime(const ConversionTime value=ConversionTime27_5ms);
```
___Arguments___
* `value` [ConversionTime] : The conversion time can be one of the following:
	* 27.5 ms
	* 55 ms
	* 110 ms
	* 220 ms

	This can be set by the ConversionTime enum:
	```c
	enum ConversionTime : uint8_t {
	  ConversionTime27_5ms = 0b00,
	  ConversionTime55ms = 0b01,
	  ConversionTime110ms = 0b10,
	  ConversionTime220ms = 0b11,
	};
	```
	The default is 27.5 ms. The same value as after resetting the device.

```c
  ConversionTime getConversionTime();
```
___Returns___
* [ConversionTime] : The conversion time as described above.

```c
  void setFaultsUntilAlert(const ConsecutiveFaults value=Fault_1);
```
___Arguments___
* `value` [ConsecutiveFaults] :  The number of consecutive faults before the alert pin is triggered. It can be a number between 1-4.
	This can be set by the ConsecutiveFaults enum:
	```c
	enum ConsecutiveFaults : uint8_t {
	  Fault_1 = 0b00,
	  Fault_2 = 0b01,
	  Fault_3 = 0b10,
	  Fault_4 = 0b11,
	};
	```
	The default is 1 fault. The same value as after resetting the device.

```c
   ConsecutiveFaults getFaultsUntilAlert();
```
___Returns___
* [ConsecutiveFaults] : The number of consecutive faults until an alert is triggered as described above.

```c
  setAlertPolarity(const bool isHigh=false)
```
___Arguments___
* `isHigh` [bool] : Setting this to true causes the alert pin to float high (using an external pull-up resistor) in case of an alert. Setting it to false will cause the pin to be pulled low in case of an alert. By default the alert pin will be pulled low.

```c
  bool getAlertPolarity();
```
___Returns___
* [bool] : Returns whether the alert pin is pulled high (`true`) or pulled low (`false`) in case of an alert.

```c
  void setAlertMode(const bool isInterrupt=false);
```
___Arguments___
* `isInterrupt` [bool] : When set to `true`, the alert pin is SMBus compatible. Once the alert is active, the device will wait for an `SMBus Alert` command on the bus, then it will release the pin. If set to false, it pin will stay triggered until the alert condition is gone. By default, the alert pin is not SMBus compatible.

```c
  bool getAlertMode();
```
___Returns___
* [bool] : `true` if the alert is SMBus compatible and needs clearing, `false` if the alert stays active while the alert condition is active.


```c
  void setHighTemperatureLimitRaw(const int16_t value=0x5000);
  void setHighTemperatureLimitCelsius(const float value=80.f);
```
___Arguments___
* `value` [int16_t] : If the temperature goes above this value, an alert will be triggered. The scale is the same as `getTemperatureRaw()`. To disable the temperature alert, set the high value value to `0x7FF0` and the low value to `0x8000`. The default (also after reset) is 80 °C (`0x5000`).
* `value`[float] The same as above, but in units of °C. The default (also after reset) is 80 °C.

```c
  void setLowTemperatureLimitRaw(const int16_t value=0x4b00);
  void setLowTemperatureLimitCelsius(const float value=75.f);
```
This setting acts as a hysteresis. When in comparator mode, the alert, once triggered, will only be released, if the temperature drops below this value. When in interrupt mode, the alert will only trigger again, if the temperature was below this value and then rises above the high threshold.
___Arguments___
* `value` [int16_t] : If the temperature goes below this value, the alert will be released. The scale is the same as `getTemperatureRaw()`. To disable the alert, set the low value to `0x8000` and the high value to `0x7FF0`. The default (also after reset) is 75 °C (`0x4b00`).
* `value`[float] The same as above, but in units of °C. The default (also after reset) is 75 °C.

```c
  int16_t getLowTemperatureLimitRaw();
  float getLowTemperatureLimitCelsius();
```
___Returns___
* [int16_t] :  See `setLowTemperatureLimitRaw()`.
* [float] :  See `setLowTemperatureLimitCelsius()`.

```c
  int16_t getHighTemperatureLimitRaw();
  float getHighTemperatureLimitCelsius();
```
___Returns___
* [int16_t] :  See `setHighTemperatureLimitRaw()`.
* [float] :  See `setLowTemperatureLimitCelsius()`.

## Examples
This is an example, which runs on the Arduino platform. The example was tested on an STM32F103RC using [Arduino_STM32](https://github.com/rogerclarkmelbourne/Arduino_STM32), but it should work on any Arduino platform.
```c
#include <TMP1075.h>    // Use quotation marks if the library is in the same folder as the sketch
// #include "TMP1075/src/TMP1075.h"    // Use this import, if you copied the library folder into the sketch folder
#include <Arduino.h>

//TwoWire wire = TwoWire(1, I2C_REMAP, 400*1000);     // Arduino_STM32 use I²C 1 with alternative pin mapping
TwoWire wire = Wire;
TMP1075::TMP1075 tmp1075 = TMP1075::TMP1075(wire);    // The library uses the namespace TMP1075

void setup() {
  Serial.begin(115200);
  tmp1075.begin();    // Calls Wire.begin(addr)
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
```

## Versioning

I use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/PatrickBaus/Arduino-TMP1075/tags). 

## Authors

* **Patrick Baus** - *Initial work* - [PatrickBaus](https://github.com/PatrickBaus)

## License

This project is licensed under the GPL v3 license - see the [LICENSE](LICENSE) file for details

