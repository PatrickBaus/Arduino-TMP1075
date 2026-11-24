#include "Wire.h"

TwoWire Wire;

void TwoWire::begin() {
  this->beginCalls++;
}

void TwoWire::beginTransmission(const uint8_t address) {
  this->transmitting = true;
  this->current = Transmission{address, {}, true};
}

size_t TwoWire::write(const uint8_t value) {
  if (!this->transmitting) {
    return 0;
  }
  this->current.data.push_back(value);
  return 1;
}

uint8_t TwoWire::endTransmission(const bool sendStop) {
  this->current.sendStop = sendStop;
  this->transmissions.push_back(this->current);
  this->transmitting = false;

  uint8_t status = 0;
  if (!this->endTransmissionStatus.empty()) {
    status = this->endTransmissionStatus.front();
    this->endTransmissionStatus.pop_front();
  }
  return status;
}

uint8_t TwoWire::requestFrom(const uint8_t address, const uint8_t quantity, const uint8_t sendStop) {
  this->requests.push_back(Request{address, quantity, sendStop != 0});

  // Like the real implementation, a new request discards stale bytes.
  this->rxBuffer.clear();
  if (!this->responses.empty()) {
    const std::vector<uint8_t> response = this->responses.front();
    this->responses.pop_front();
    for (size_t i = 0; i < response.size() && i < quantity; i++) {
      this->rxBuffer.push_back(response[i]);
    }
  }
  return (uint8_t)this->rxBuffer.size();
}

int TwoWire::available() {
  return (int)this->rxBuffer.size();
}

int TwoWire::read() {
  this->readCalls++;
  if (this->rxBuffer.empty()) {
    return -1;
  }
  const uint8_t value = this->rxBuffer.front();
  this->rxBuffer.pop_front();
  return value;
}

void TwoWire::queueResponse(const std::vector<uint8_t>& bytes) {
  this->responses.push_back(bytes);
}

void TwoWire::queueEndTransmissionStatus(const uint8_t status) {
  this->endTransmissionStatus.push_back(status);
}

void TwoWire::clearTraffic() {
  this->transmissions.clear();
  this->requests.clear();
  this->readCalls = 0;
}
