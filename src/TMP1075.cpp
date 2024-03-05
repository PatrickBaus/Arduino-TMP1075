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
#include "TMP1075.h"

namespace TMP1075 {
  TMP1075::TMP1075(TwoWire& _wire, const uint8_t _i2cAddress): wire(_wire), i2cAddress(_i2cAddress), configRegister(0x00FF) {}

  void TMP1075::begin()
  {
    this->configRegister = (uint8_t)(this->readRegister(0x01) >> 8);
  }

  void TMP1075::writeRegister(const uint8_t pointerRegister, const uint16_t value) {
    this->wire.beginTransmission(this->i2cAddress);
    this->wire.write(pointerRegister);
    this->wire.write((value >> 8) & 0xFF);
    this->wire.write((value >> 0) & 0xFF);
    this->wire.endTransmission();
  }

  void TMP1075::writeRegister(const uint8_t pointerRegister, const uint8_t value) {
    this->wire.beginTransmission(this->i2cAddress);
    this->wire.write(pointerRegister);
    this->wire.write(value);
    this->wire.endTransmission();
  }

  uint16_t TMP1075::readRegister(const uint8_t pointerRegister) {
    this->wire.beginTransmission(this->i2cAddress);
    this->wire.write(pointerRegister);
    this->wire.endTransmission(false);
    this->wire.requestFrom(this->i2cAddress, (uint8_t)2, (uint8_t)true);
    return this->wire.read() << 8 | this->wire.read();
  }

  int16_t TMP1075::getTemperatureRaw() {
    return (int16_t)this->readRegister(0x00);
  }

  float TMP1075::getTemperatureCelsius() {
    return TMP1075::convertToCelsius(this->getTemperatureRaw());
  }

  void TMP1075::startConversion() {
    // There is no need to save the OS bit to the cache, because it will always be zero when read.
    this->writeRegister(0x01, (uint8_t)(this->configRegister | (1 << Offsets::OS)));
  }

  bool TMP1075::getConversionMode() const {
    return (this->configRegister >> Offsets::SD) & 0b1;
  }

  void TMP1075::setConversionMode(const bool isSingleShot) {
    if (((configRegister >> Offsets::SD) & 0b1) != isSingleShot) {
      this->configRegister  = (this->configRegister & ~(1 << Offsets::SD)) | (isSingleShot << Offsets::SD);
      this->writeRegister(0x01, this->configRegister);
    }
  }

  ConversionTime TMP1075::getConversionTime() const {
    return ConversionTime((this->configRegister >> Offsets::R) & 0b11);
  }

  void TMP1075::setConversionTime(const ConversionTime value) {
    if (ConversionTime((configRegister >> Offsets::R) & 0b11) != value) {
      this->configRegister  = (this->configRegister & ~(11 << Offsets::R)) | (value << Offsets::R);
      this->writeRegister(0x01, this->configRegister);
    }
  }

  ConsecutiveFaults TMP1075::getFaultsUntilAlert() const {
    return ConsecutiveFaults((this->configRegister >> Offsets::F) & 0b11);
  }

  void TMP1075::setFaultsUntilAlert(const ConsecutiveFaults value) {
    if (ConsecutiveFaults((configRegister >> Offsets::F) & 0b11) != value) {
      this->configRegister  = (this->configRegister & ~(11 << Offsets::F)) | (value << Offsets::F);
      this->writeRegister(0x01, this->configRegister);
    }
  }

  bool TMP1075::getAlertPolarity() const {
    return (this->configRegister >> Offsets::POL) & 0b1;
  }

  void TMP1075::setAlertPolarity(const bool isHigh) {
    if (((configRegister >> Offsets::POL) & 0b1) != isHigh) {
      this->configRegister  = (this->configRegister & ~(1 << Offsets::POL)) | (isHigh << Offsets::POL);
      this->writeRegister(0x01, this->configRegister);
    }
  }

  bool TMP1075::getAlertMode() const {
    return (this->configRegister >> Offsets::TM) & 0b1;
  }

  void TMP1075::setAlertMode(const bool isInterrupt) {
    if (((configRegister >> Offsets::TM) & 0b1) != isInterrupt) {
      this->configRegister  = (this->configRegister & ~(1 << Offsets::TM)) | (isInterrupt << Offsets::TM);
      this->writeRegister(0x01, this->configRegister);
    }
  }

  int16_t TMP1075::getLowTemperatureLimitRaw() {
    return (int16_t)(this->readRegister(0x02));
  }

  void TMP1075::setLowTemperatureLimitRaw(const int16_t value) {
    this->writeRegister(0x02, (uint16_t)value);
  }

  void TMP1075::setLowTemperatureLimitCelsius(const float value) {
    this->setLowTemperatureLimitRaw(TMP1075::convertFromCelsius(value));
  }

  float TMP1075::getLowTemperatureLimitCelsius() {
    return TMP1075::convertToCelsius(this->getLowTemperatureLimitRaw());
  }

  int16_t TMP1075::getHighTemperatureLimitRaw() {
    return (int16_t)this->readRegister(0x03);
  }

  float TMP1075::getHighTemperatureLimitCelsius() {
    return TMP1075::convertToCelsius(this->getHighTemperatureLimitRaw());
  }

  void TMP1075::setHighTemperatureLimitRaw(const int16_t value) {
    this->writeRegister(0x03, (uint16_t)value);
  }

  void TMP1075::setHighTemperatureLimitCelsius(const float value) {
    this->setHighTemperatureLimitRaw(TMP1075::convertFromCelsius(value));
  }

  uint16_t TMP1075::getDeviceId() {
    return (uint16_t)(this->readRegister(0x0F));
  }

  float TMP1075::convertToCelsius(const int16_t value) {
    return 0.0625f * float(value) / 16;
  }

  int16_t TMP1075::convertFromCelsius(const float value) {
    return  (int16_t)(value * 16 / 0.0625f);
  }
}   // namespace TMP1075
