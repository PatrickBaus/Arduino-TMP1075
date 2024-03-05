/**
# ##### BEGIN GPL LICENSE BLOCK #####
#
# Copyright (C) 2020  Patrick Baus
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# ##### END GPL LICENSE BLOCK #####
*/
#ifndef TMP1075_H
#define TMP1075_H

#include <stdint.h>   // uint16_t, etc.

#include <Arduino.h>
#include <Wire.h>     // TwoWire

namespace TMP1075 {
  enum ConsecutiveFaults : uint8_t {
    Fault_1 = 0b00,
    Fault_2 = 0b01,
    Fault_3 = 0b10,
    Fault_4 = 0b11,
  };

  enum ConversionTime : uint8_t {
    ConversionTime27_5ms = 0b00,
    ConversionTime55ms = 0b01,
    ConversionTime110ms = 0b10,
    ConversionTime220ms = 0b11,
  };

  enum Offsets: uint8_t {
    OS = 7,
    R  = 5,
    F  = 3,
    POL = 2,
    TM = 1,
    SD = 0,
  };

  class TMP1075
  {
    public:
      // i2cAddress is the default address when A0, A1 and A2 is tied low
      explicit TMP1075(TwoWire& wire, uint8_t i2cAddress = 0x48);

      void begin();
      int16_t getTemperatureRaw();
      float getTemperatureCelsius();
      void startConversion();
      bool getConversionMode() const;
      void setConversionMode(const bool isSingleShot=false);
      ConversionTime getConversionTime() const;
      void setConversionTime(ConversionTime value=ConversionTime27_5ms);
      ConsecutiveFaults getFaultsUntilAlert() const;
      void setFaultsUntilAlert(ConsecutiveFaults value=Fault_1);
      bool getAlertPolarity() const;
      void setAlertPolarity(const bool isHigh=false);
      bool getAlertMode() const;
      void setAlertMode(const bool isInterrupt=false);
      int16_t getLowTemperatureLimitRaw();
      float getLowTemperatureLimitCelsius();
      void setLowTemperatureLimitRaw(int16_t value=0x4b00);
      void setLowTemperatureLimitCelsius(float value=75.f);
      int16_t getHighTemperatureLimitRaw();
      float getHighTemperatureLimitCelsius();
      void setHighTemperatureLimitRaw(int16_t value=0x5000);
      void setHighTemperatureLimitCelsius(float value=80.f);
      uint16_t getDeviceId();
    private:
      TwoWire& wire;
      void writeRegister(uint8_t reg, uint16_t value);
      void writeRegister(uint8_t reg, uint8_t value);
      static float convertToCelsius(int16_t value);
      static int16_t convertFromCelsius(float value);
      uint16_t readRegister(uint8_t reg);
      uint8_t i2cAddress;
      uint8_t configRegister;
  };
}
#endif    // TMP1075_H
