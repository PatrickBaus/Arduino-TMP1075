// Host-side fake of the Arduino TwoWire (I2C) class.
//
// It records everything the code under test puts on the bus and lets a test
// script what the "device" answers, including NACKs and short reads.
#ifndef MOCK_WIRE_H
#define MOCK_WIRE_H

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <vector>

class TwoWire {
  public:
    // One beginTransmission()/endTransmission() pair, i.e. one I2C write.
    struct Transmission {
      uint8_t address;
      std::vector<uint8_t> data;
      bool sendStop;
    };

    // One requestFrom() call, i.e. one I2C read.
    struct Request {
      uint8_t address;
      uint8_t quantity;
      bool sendStop;
    };

    // ---- Arduino API (the subset used by the library) ----
    void begin();
    void beginTransmission(uint8_t address);
    size_t write(uint8_t value);
    uint8_t endTransmission(bool sendStop = true);
    uint8_t requestFrom(uint8_t address, uint8_t quantity, uint8_t sendStop);
    int available();
    int read();

    // ---- Test API ----
    // Queue the bytes the device returns for the next requestFrom(). Queueing
    // fewer bytes than the library asks for simulates a short read. Without a
    // queued response the device does not answer at all (address NACK).
    void queueResponse(const std::vector<uint8_t>& bytes);
    // Queue the status returned by the next endTransmission(), e.g. 2 for an
    // address NACK. The default is 0 (success).
    void queueEndTransmissionStatus(uint8_t status);
    // Forget the recorded traffic, keep the scripted responses.
    void clearTraffic();

    std::vector<Transmission> transmissions;  // all completed writes, in order
    std::vector<Request> requests;            // all reads, in order
    unsigned beginCalls = 0;
    unsigned readCalls = 0;                   // number of read() calls

  private:
    bool transmitting = false;
    Transmission current{};
    std::deque<std::vector<uint8_t>> responses;
    std::deque<uint8_t> endTransmissionStatus;
    std::deque<uint8_t> rxBuffer;
};

extern TwoWire Wire;

#endif  // MOCK_WIRE_H
