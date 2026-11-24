// Host-based tests for the TMP1075 library.
//
// All expected values are taken from the TMP1075 datasheet (SBOS854):
// register map, configuration register layout and the temperature data format
// table (12 bit, left justified, two's complement, 0.0625 degC per LSB).
#include <doctest.h>

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include <TMP1075.h>

// Print the bytes on the wire as hex, if an assertion fails
namespace doctest {
  template<> struct StringMaker<std::vector<uint8_t>> {
    static String convert(const std::vector<uint8_t>& bytes) {
      std::string result = "{";
      for (size_t i = 0; i < bytes.size(); i++) {
        char buffer[8];
        std::snprintf(buffer, sizeof(buffer), "%s0x%02X", i == 0 ? "" : ", ", bytes[i]);
        result += buffer;
      }
      result += "}";
      return String(result.c_str());
    }
  };
}  // namespace doctest

namespace {
  using Bytes = std::vector<uint8_t>;

  const uint8_t DEFAULT_ADDRESS = 0x48;

  // Register pointers
  const uint8_t REG_TEMP = 0x00;
  const uint8_t REG_CFGR = 0x01;
  const uint8_t REG_LLIM = 0x02;
  const uint8_t REG_HLIM = 0x03;
  const uint8_t REG_DIEID = 0x0F;

  // Bits of the configuration register (upper byte of CFGR)
  const uint8_t CFG_OS = 0x80;
  const uint8_t CFG_R1 = 0x40;
  const uint8_t CFG_R0 = 0x20;
  const uint8_t CFG_F1 = 0x10;
  const uint8_t CFG_F0 = 0x08;
  const uint8_t CFG_POL = 0x04;
  const uint8_t CFG_TM = 0x02;
  const uint8_t CFG_SD = 0x01;

  struct TemperatureCode {
    float celsius;
    uint16_t code;
  };

  // Datasheet table "Temperature Data Format", extended by the reset values of
  // the limit registers (75 degC, 80 degC).
  const TemperatureCode TEMPERATURE_TABLE[] = {
    { 127.9375f, 0x7FF0},
    { 100.0f,    0x6400},
    {  80.0f,    0x5000},
    {  75.0f,    0x4B00},
    {  50.0f,    0x3200},
    {  25.0f,    0x1900},
    {   0.25f,   0x0040},
    {   0.0625f, 0x0010},
    {   0.0f,    0x0000},
    {  -0.0625f, 0xFFF0},
    {  -0.25f,   0xFFC0},
    { -25.0f,    0xE700},
    { -50.0f,    0xCE00},
    {-128.0f,    0x8000},
  };

  // A sensor on a fake bus. `config` is the upper byte of CFGR that the fake
  // device returns when the library synchronises its cache in begin().
  struct Fixture {
    TwoWire wire;
    TMP1075::TMP1075 sensor;

    explicit Fixture(const uint8_t address = DEFAULT_ADDRESS): wire(), sensor(wire, address) {}

    void begin(const uint8_t config) {
      // The lower byte of CFGR always reads 0xFF on the TMP1075
      this->wire.queueResponse({config, 0xFF});
      this->sensor.begin();
      this->wire.clearTraffic();
    }

    // Asserts that exactly one register read of `reg` happened: a write of the
    // pointer byte without STOP, followed by a two byte read with STOP.
    void checkSingleRead(const uint8_t reg, const uint8_t address = DEFAULT_ADDRESS) {
      REQUIRE(this->wire.transmissions.size() == 1);
      CHECK(this->wire.transmissions[0].address == address);
      CHECK(this->wire.transmissions[0].data == Bytes{reg});
      CHECK_FALSE(this->wire.transmissions[0].sendStop);
      REQUIRE(this->wire.requests.size() == 1);
      CHECK(this->wire.requests[0].address == address);
      CHECK(this->wire.requests[0].quantity == 2);
      CHECK(this->wire.requests[0].sendStop);
    }

    // Asserts that exactly one write happened and that these bytes (pointer
    // register followed by the payload) went over the wire.
    void checkSingleWrite(const Bytes& expected, const uint8_t address = DEFAULT_ADDRESS) {
      REQUIRE(this->wire.transmissions.size() == 1);
      CHECK(this->wire.transmissions[0].address == address);
      CHECK(this->wire.transmissions[0].data == expected);
      CHECK(this->wire.transmissions[0].sendStop);
      CHECK(this->wire.requests.empty());
    }
  };
}  // namespace

TEST_SUITE("construction and begin()") {
  TEST_CASE("the constructor does not touch the bus") {
    Fixture f;
    CHECK(f.wire.transmissions.empty());
    CHECK(f.wire.requests.empty());
    CHECK(f.wire.beginCalls == 0);
  }

  TEST_CASE("the cached configuration matches the power-on reset value of the device") {
    // CFGR resets to 0x00FF: continuous conversion, 27.5 ms, 1 fault,
    // ALERT active low, comparator mode.
    Fixture f;
    CHECK_FALSE(f.sensor.getConversionMode());
    CHECK(f.sensor.getConversionTime() == TMP1075::ConversionTime27_5ms);
    CHECK(f.sensor.getFaultsUntilAlert() == TMP1075::Fault_1);
    CHECK_FALSE(f.sensor.getAlertPolarity());
    CHECK_FALSE(f.sensor.getAlertMode());
    CHECK(f.wire.transmissions.empty());
  }

  TEST_CASE("settings can be changed without calling begin() on a freshly reset device") {
    Fixture f;
    f.sensor.setConversionMode(true);
    f.checkSingleWrite({REG_CFGR, CFG_SD});
  }

  TEST_CASE("begin() reads the configuration register") {
    Fixture f;
    f.wire.queueResponse({0x00, 0xFF});
    f.sensor.begin();
    f.checkSingleRead(REG_CFGR);
    CHECK(f.wire.readCalls == 2);
    // begin() must not initialise the bus, that is the job of the sketch
    CHECK(f.wire.beginCalls == 0);
  }

  TEST_CASE("begin() caches the upper byte of the configuration register") {
    Fixture f;

    SUBCASE("reset value") {
      f.begin(0x00);
      CHECK_FALSE(f.sensor.getConversionMode());
      CHECK(f.sensor.getConversionTime() == TMP1075::ConversionTime27_5ms);
      CHECK(f.sensor.getFaultsUntilAlert() == TMP1075::Fault_1);
      CHECK_FALSE(f.sensor.getAlertPolarity());
      CHECK_FALSE(f.sensor.getAlertMode());
    }

    SUBCASE("everything set") {
      f.begin(CFG_R1 | CFG_R0 | CFG_F1 | CFG_F0 | CFG_POL | CFG_TM | CFG_SD);
      CHECK(f.sensor.getConversionMode());
      CHECK(f.sensor.getConversionTime() == TMP1075::ConversionTime220ms);
      CHECK(f.sensor.getFaultsUntilAlert() == TMP1075::Fault_4);
      CHECK(f.sensor.getAlertPolarity());
      CHECK(f.sensor.getAlertMode());
    }

    SUBCASE("each bit maps to the documented setting") {
      f.begin(CFG_SD);
      CHECK(f.sensor.getConversionMode());
      CHECK(f.sensor.getConversionTime() == TMP1075::ConversionTime27_5ms);
      CHECK(f.sensor.getFaultsUntilAlert() == TMP1075::Fault_1);
      CHECK_FALSE(f.sensor.getAlertPolarity());
      CHECK_FALSE(f.sensor.getAlertMode());

      f.begin(CFG_TM);
      CHECK(f.sensor.getAlertMode());
      CHECK_FALSE(f.sensor.getConversionMode());
      CHECK_FALSE(f.sensor.getAlertPolarity());

      f.begin(CFG_POL);
      CHECK(f.sensor.getAlertPolarity());
      CHECK_FALSE(f.sensor.getAlertMode());
      CHECK(f.sensor.getFaultsUntilAlert() == TMP1075::Fault_1);

      f.begin(CFG_F0);
      CHECK(f.sensor.getFaultsUntilAlert() == TMP1075::Fault_2);
      f.begin(CFG_F1);
      CHECK(f.sensor.getFaultsUntilAlert() == TMP1075::Fault_3);
      CHECK(f.sensor.getConversionTime() == TMP1075::ConversionTime27_5ms);

      f.begin(CFG_R0);
      CHECK(f.sensor.getConversionTime() == TMP1075::ConversionTime55ms);
      f.begin(CFG_R1);
      CHECK(f.sensor.getConversionTime() == TMP1075::ConversionTime110ms);
      CHECK(f.sensor.getFaultsUntilAlert() == TMP1075::Fault_1);
    }

    SUBCASE("the lower byte is ignored") {
      // The TMP1075N has an 8 bit configuration register, the second byte is not 0xFF there
      f.wire.queueResponse({CFG_SD, 0x00});
      f.sensor.begin();
      CHECK(f.sensor.getConversionMode());
      CHECK(f.sensor.getConversionTime() == TMP1075::ConversionTime27_5ms);
    }
  }

  TEST_CASE("a non-default I2C address is used for reads and writes") {
    Fixture f(0x4F);
    f.wire.queueResponse({0x00, 0xFF});
    f.sensor.begin();
    f.checkSingleRead(REG_CFGR, 0x4F);
    f.wire.clearTraffic();

    f.sensor.setAlertMode(true);
    f.checkSingleWrite({REG_CFGR, CFG_TM}, 0x4F);
  }
}

TEST_SUITE("temperature") {
  TEST_CASE("getTemperatureRaw() returns the register content, MSB first") {
    Fixture f;
    f.begin(0x00);
    f.wire.queueResponse({0x12, 0x30});
    CHECK(f.sensor.getTemperatureRaw() == 0x1230);
    f.checkSingleRead(REG_TEMP);
    CHECK(f.wire.readCalls == 2);
  }

  TEST_CASE("datasheet temperature table") {
    for (const TemperatureCode& entry : TEMPERATURE_TABLE) {
      CAPTURE(entry.celsius);
      Fixture f;
      f.begin(0x00);
      const Bytes response = {(uint8_t)(entry.code >> 8), (uint8_t)(entry.code & 0xFF)};

      f.wire.queueResponse(response);
      CHECK(f.sensor.getTemperatureRaw() == (int16_t)entry.code);

      f.wire.queueResponse(response);
      CHECK(f.sensor.getTemperatureCelsius() == entry.celsius);  // all values are exact in binary
    }
  }

  TEST_CASE("negative temperatures keep their sign") {
    Fixture f;
    f.begin(0x00);
    f.wire.queueResponse({0xFF, 0xF0});
    const int16_t raw = f.sensor.getTemperatureRaw();
    CHECK(raw == -16);
    CHECK(raw / 16 * 0.0625f == -0.0625f);  // The conversion documented in the README
  }

  TEST_CASE("every call triggers a new read") {
    Fixture f;
    f.begin(0x00);
    f.wire.queueResponse({0x19, 0x00});
    f.wire.queueResponse({0x19, 0x10});
    CHECK(f.sensor.getTemperatureCelsius() == 25.0f);
    CHECK(f.sensor.getTemperatureCelsius() == 25.0625f);
    CHECK(f.wire.requests.size() == 2);
  }
}

TEST_SUITE("bus errors") {
  TEST_CASE("a device that does not answer yields 0xFFFF") {
    Fixture f;
    f.begin(0x00);
    // Address NACK: endTransmission() fails and no bytes are received
    f.wire.queueEndTransmissionStatus(2);
    CHECK(f.sensor.getTemperatureRaw() == -1);
    f.checkSingleRead(REG_TEMP);
    // The (empty) receive buffer must not be read
    CHECK(f.wire.readCalls == 0);
  }

  TEST_CASE("a short read yields 0xFFFF") {
    Fixture f;
    f.begin(0x00);
    f.wire.queueResponse({0x19});
    CHECK(f.sensor.getTemperatureRaw() == -1);
    CHECK(f.wire.readCalls == 0);

    // The next read is not affected by the stale byte
    f.wire.queueResponse({0x32, 0x00});
    CHECK(f.sensor.getTemperatureCelsius() == 50.0f);
  }

  TEST_CASE("the error value can be told apart from a temperature") {
    // The four LSBs of the temperature and limit registers always read 0
    Fixture f;
    f.begin(0x00);
    const int16_t raw = f.sensor.getTemperatureRaw();
    CHECK((raw & 0x000F) != 0);
  }

  TEST_CASE("all readers report the error value") {
    Fixture f;
    f.begin(0x00);
    CHECK(f.sensor.getLowTemperatureLimitRaw() == -1);
    CHECK(f.sensor.getHighTemperatureLimitRaw() == -1);
    CHECK(f.sensor.getDeviceId() == 0xFFFF);
    CHECK(f.wire.readCalls == 0);
  }

  TEST_CASE("writes are sent even if the device does not acknowledge") {
    Fixture f;
    f.begin(0x00);
    f.wire.queueEndTransmissionStatus(2);
    f.sensor.setHighTemperatureLimitRaw(0x5000);
    f.checkSingleWrite({REG_HLIM, 0x50, 0x00});
  }
}

TEST_SUITE("conversion mode and one-shot") {
  TEST_CASE("setConversionMode() sets and clears the SD bit") {
    Fixture f;
    f.begin(0x00);

    f.sensor.setConversionMode(true);
    f.checkSingleWrite({REG_CFGR, CFG_SD});
    CHECK(f.sensor.getConversionMode());

    f.wire.clearTraffic();
    f.sensor.setConversionMode(false);
    f.checkSingleWrite({REG_CFGR, 0x00});
    CHECK_FALSE(f.sensor.getConversionMode());
  }

  TEST_CASE("setConversionMode() defaults to continuous conversion") {
    Fixture f;
    f.begin(CFG_SD);
    f.sensor.setConversionMode();
    f.checkSingleWrite({REG_CFGR, 0x00});
  }

  TEST_CASE("setConversionMode() does not write if nothing changes") {
    Fixture f;
    f.begin(CFG_SD);
    f.sensor.setConversionMode(true);
    CHECK(f.wire.transmissions.empty());

    f.begin(0x00);
    f.sensor.setConversionMode(false);
    CHECK(f.wire.transmissions.empty());
  }

  TEST_CASE("setConversionMode() keeps the other settings") {
    const uint8_t others = CFG_R1 | CFG_R0 | CFG_F1 | CFG_F0 | CFG_POL | CFG_TM;
    Fixture f;
    f.begin(others);
    f.sensor.setConversionMode(true);
    f.checkSingleWrite({REG_CFGR, (uint8_t)(others | CFG_SD)});

    f.wire.clearTraffic();
    f.sensor.setConversionMode(false);
    f.checkSingleWrite({REG_CFGR, others});
  }

  TEST_CASE("startConversion() sets the OS bit and keeps the configuration") {
    Fixture f;
    f.begin(CFG_SD | CFG_R0);
    f.sensor.startConversion();
    f.checkSingleWrite({REG_CFGR, (uint8_t)(CFG_OS | CFG_SD | CFG_R0)});
  }

  TEST_CASE("the OS bit is not cached") {
    // OS always reads 0, it must not leak into later writes of the configuration
    Fixture f;
    f.begin(CFG_SD);
    f.sensor.startConversion();
    f.wire.clearTraffic();

    f.sensor.setAlertMode(true);
    f.checkSingleWrite({REG_CFGR, (uint8_t)(CFG_SD | CFG_TM)});

    // Every call starts a new conversion
    f.wire.clearTraffic();
    f.sensor.startConversion();
    f.checkSingleWrite({REG_CFGR, (uint8_t)(CFG_OS | CFG_SD | CFG_TM)});
  }
}

TEST_SUITE("conversion time") {
  TEST_CASE("setConversionTime() writes the R[1:0] bits") {
    struct Case { TMP1075::ConversionTime value; uint8_t bits; };
    const Case cases[] = {
      {TMP1075::ConversionTime55ms, CFG_R0},
      {TMP1075::ConversionTime110ms, CFG_R1},
      {TMP1075::ConversionTime220ms, CFG_R1 | CFG_R0},
    };
    for (const Case& c : cases) {
      CAPTURE(c.bits);
      Fixture f;
      f.begin(0x00);
      f.sensor.setConversionTime(c.value);
      f.checkSingleWrite({REG_CFGR, c.bits});
      CHECK(f.sensor.getConversionTime() == c.value);
    }
  }

  TEST_CASE("setConversionTime() clears bits that were set before") {
    Fixture f;
    f.begin(CFG_R1 | CFG_R0);
    f.sensor.setConversionTime(TMP1075::ConversionTime55ms);
    f.checkSingleWrite({REG_CFGR, CFG_R0});

    f.wire.clearTraffic();
    f.sensor.setConversionTime(TMP1075::ConversionTime110ms);
    f.checkSingleWrite({REG_CFGR, CFG_R1});

    f.wire.clearTraffic();
    f.sensor.setConversionTime();  // The default is the reset value of 27.5 ms
    f.checkSingleWrite({REG_CFGR, 0x00});
    CHECK(f.sensor.getConversionTime() == TMP1075::ConversionTime27_5ms);
  }

  TEST_CASE("setConversionTime() keeps the other settings") {
    const uint8_t others = CFG_F1 | CFG_F0 | CFG_POL | CFG_TM | CFG_SD;
    Fixture f;
    f.begin(others | CFG_R1 | CFG_R0);
    f.sensor.setConversionTime(TMP1075::ConversionTime27_5ms);
    f.checkSingleWrite({REG_CFGR, others});
    CHECK(f.sensor.getFaultsUntilAlert() == TMP1075::Fault_4);

    f.wire.clearTraffic();
    f.sensor.setConversionTime(TMP1075::ConversionTime220ms);
    f.checkSingleWrite({REG_CFGR, (uint8_t)(others | CFG_R1 | CFG_R0)});
  }

  TEST_CASE("setConversionTime() does not write if nothing changes") {
    Fixture f;
    f.begin(CFG_R1);
    f.sensor.setConversionTime(TMP1075::ConversionTime110ms);
    CHECK(f.wire.transmissions.empty());
  }
}

TEST_SUITE("fault queue") {
  TEST_CASE("setFaultsUntilAlert() writes the F[1:0] bits") {
    struct Case { TMP1075::ConsecutiveFaults value; uint8_t bits; };
    const Case cases[] = {
      {TMP1075::Fault_2, CFG_F0},
      {TMP1075::Fault_3, CFG_F1},
      {TMP1075::Fault_4, CFG_F1 | CFG_F0},
    };
    for (const Case& c : cases) {
      CAPTURE(c.bits);
      Fixture f;
      f.begin(0x00);
      f.sensor.setFaultsUntilAlert(c.value);
      f.checkSingleWrite({REG_CFGR, c.bits});
      CHECK(f.sensor.getFaultsUntilAlert() == c.value);
    }
  }

  TEST_CASE("setFaultsUntilAlert() clears bits that were set before") {
    Fixture f;
    f.begin(CFG_F1 | CFG_F0);
    f.sensor.setFaultsUntilAlert(TMP1075::Fault_2);
    f.checkSingleWrite({REG_CFGR, CFG_F0});

    f.wire.clearTraffic();
    f.sensor.setFaultsUntilAlert(TMP1075::Fault_3);
    f.checkSingleWrite({REG_CFGR, CFG_F1});

    f.wire.clearTraffic();
    f.sensor.setFaultsUntilAlert();  // The default is the reset value of 1 fault
    f.checkSingleWrite({REG_CFGR, 0x00});
    CHECK(f.sensor.getFaultsUntilAlert() == TMP1075::Fault_1);
  }

  TEST_CASE("setFaultsUntilAlert() keeps the other settings") {
    const uint8_t others = CFG_R1 | CFG_R0 | CFG_POL | CFG_TM | CFG_SD;
    Fixture f;
    f.begin(others);
    f.sensor.setFaultsUntilAlert(TMP1075::Fault_2);
    f.checkSingleWrite({REG_CFGR, (uint8_t)(others | CFG_F0)});
    // In particular the conversion time, which sits right next to the fault queue
    CHECK(f.sensor.getConversionTime() == TMP1075::ConversionTime220ms);

    f.wire.clearTraffic();
    f.sensor.setFaultsUntilAlert(TMP1075::Fault_1);
    f.checkSingleWrite({REG_CFGR, others});
    CHECK(f.sensor.getConversionTime() == TMP1075::ConversionTime220ms);
  }

  TEST_CASE("setFaultsUntilAlert() does not write if nothing changes") {
    Fixture f;
    f.begin(CFG_F0);
    f.sensor.setFaultsUntilAlert(TMP1075::Fault_2);
    CHECK(f.wire.transmissions.empty());
  }
}

TEST_SUITE("alert pin") {
  TEST_CASE("setAlertPolarity() sets and clears the POL bit") {
    Fixture f;
    f.begin(0x00);
    f.sensor.setAlertPolarity(true);
    f.checkSingleWrite({REG_CFGR, CFG_POL});
    CHECK(f.sensor.getAlertPolarity());

    f.wire.clearTraffic();
    f.sensor.setAlertPolarity();  // The default is active low
    f.checkSingleWrite({REG_CFGR, 0x00});
    CHECK_FALSE(f.sensor.getAlertPolarity());
  }

  TEST_CASE("setAlertPolarity() keeps the other settings") {
    const uint8_t others = CFG_R1 | CFG_R0 | CFG_F1 | CFG_F0 | CFG_TM | CFG_SD;
    Fixture f;
    f.begin(others);
    f.sensor.setAlertPolarity(true);
    f.checkSingleWrite({REG_CFGR, (uint8_t)(others | CFG_POL)});

    f.wire.clearTraffic();
    f.sensor.setAlertPolarity(false);
    f.checkSingleWrite({REG_CFGR, others});
  }

  TEST_CASE("setAlertPolarity() does not write if nothing changes") {
    Fixture f;
    f.begin(CFG_POL);
    f.sensor.setAlertPolarity(true);
    CHECK(f.wire.transmissions.empty());

    f.begin(0x00);
    f.sensor.setAlertPolarity(false);
    CHECK(f.wire.transmissions.empty());
  }

  TEST_CASE("setAlertMode() sets and clears the TM bit") {
    Fixture f;
    f.begin(0x00);
    f.sensor.setAlertMode(true);
    f.checkSingleWrite({REG_CFGR, CFG_TM});
    CHECK(f.sensor.getAlertMode());

    f.wire.clearTraffic();
    f.sensor.setAlertMode();  // The default is the comparator mode
    f.checkSingleWrite({REG_CFGR, 0x00});
    CHECK_FALSE(f.sensor.getAlertMode());
  }

  TEST_CASE("setAlertMode() keeps the other settings") {
    const uint8_t others = CFG_R1 | CFG_R0 | CFG_F1 | CFG_F0 | CFG_POL | CFG_SD;
    Fixture f;
    f.begin(others);
    f.sensor.setAlertMode(true);
    f.checkSingleWrite({REG_CFGR, (uint8_t)(others | CFG_TM)});

    f.wire.clearTraffic();
    f.sensor.setAlertMode(false);
    f.checkSingleWrite({REG_CFGR, others});
  }

  TEST_CASE("setAlertMode() does not write if nothing changes") {
    Fixture f;
    f.begin(CFG_TM);
    f.sensor.setAlertMode(true);
    CHECK(f.wire.transmissions.empty());

    f.begin(0x00);
    f.sensor.setAlertMode(false);
    CHECK(f.wire.transmissions.empty());
  }
}

TEST_SUITE("temperature limits") {
  TEST_CASE("the raw limits are read MSB first") {
    Fixture f;
    f.begin(0x00);

    SUBCASE("low limit, reset value") {
      f.wire.queueResponse({0x4B, 0x00});
      CHECK(f.sensor.getLowTemperatureLimitRaw() == 0x4B00);
      f.checkSingleRead(REG_LLIM);
    }

    SUBCASE("high limit, reset value") {
      f.wire.queueResponse({0x50, 0x00});
      CHECK(f.sensor.getHighTemperatureLimitRaw() == 0x5000);
      f.checkSingleRead(REG_HLIM);
    }

    SUBCASE("low limit, alert disabled") {
      f.wire.queueResponse({0x80, 0x00});
      CHECK(f.sensor.getLowTemperatureLimitRaw() == -32768);
    }

    SUBCASE("high limit, alert disabled") {
      f.wire.queueResponse({0x7F, 0xF0});
      CHECK(f.sensor.getHighTemperatureLimitRaw() == 0x7FF0);
    }
  }

  TEST_CASE("the limits are converted to degrees Celsius") {
    for (const TemperatureCode& entry : TEMPERATURE_TABLE) {
      CAPTURE(entry.celsius);
      Fixture f;
      f.begin(0x00);
      const Bytes response = {(uint8_t)(entry.code >> 8), (uint8_t)(entry.code & 0xFF)};

      f.wire.queueResponse(response);
      CHECK(f.sensor.getLowTemperatureLimitCelsius() == entry.celsius);
      f.checkSingleRead(REG_LLIM);
      f.wire.clearTraffic();

      f.wire.queueResponse(response);
      CHECK(f.sensor.getHighTemperatureLimitCelsius() == entry.celsius);
      f.checkSingleRead(REG_HLIM);
    }
  }

  TEST_CASE("the raw limits are written MSB first") {
    Fixture f;
    f.begin(0x00);

    SUBCASE("low limit") {
      f.sensor.setLowTemperatureLimitRaw(0x1230);
      f.checkSingleWrite({REG_LLIM, 0x12, 0x30});
    }

    SUBCASE("high limit") {
      f.sensor.setHighTemperatureLimitRaw(0x1230);
      f.checkSingleWrite({REG_HLIM, 0x12, 0x30});
    }

    SUBCASE("negative low limit") {
      f.sensor.setLowTemperatureLimitRaw(-6400);  // -25 degC
      f.checkSingleWrite({REG_LLIM, 0xE7, 0x00});
    }

    SUBCASE("negative high limit") {
      f.sensor.setHighTemperatureLimitRaw(-16);  // -0.0625 degC
      f.checkSingleWrite({REG_HLIM, 0xFF, 0xF0});
    }

    SUBCASE("low limit, default is the reset value") {
      f.sensor.setLowTemperatureLimitRaw();
      f.checkSingleWrite({REG_LLIM, 0x4B, 0x00});
    }

    SUBCASE("high limit, default is the reset value") {
      f.sensor.setHighTemperatureLimitRaw();
      f.checkSingleWrite({REG_HLIM, 0x50, 0x00});
    }

    SUBCASE("alert disabled as documented in the README") {
      f.sensor.setHighTemperatureLimitRaw(0x7FF0);
      f.checkSingleWrite({REG_HLIM, 0x7F, 0xF0});
      f.wire.clearTraffic();
      f.sensor.setLowTemperatureLimitRaw((int16_t)0x8000);
      f.checkSingleWrite({REG_LLIM, 0x80, 0x00});
    }
  }

  TEST_CASE("limits in degrees Celsius are encoded according to the datasheet table") {
    for (const TemperatureCode& entry : TEMPERATURE_TABLE) {
      CAPTURE(entry.celsius);
      const uint8_t msb = (uint8_t)(entry.code >> 8);
      const uint8_t lsb = (uint8_t)(entry.code & 0xFF);
      Fixture f;
      f.begin(0x00);

      f.sensor.setLowTemperatureLimitCelsius(entry.celsius);
      f.checkSingleWrite({REG_LLIM, msb, lsb});
      f.wire.clearTraffic();

      f.sensor.setHighTemperatureLimitCelsius(entry.celsius);
      f.checkSingleWrite({REG_HLIM, msb, lsb});
    }
  }

  TEST_CASE("the Celsius setters default to the reset values") {
    Fixture f;
    f.begin(0x00);
    f.sensor.setLowTemperatureLimitCelsius();
    f.checkSingleWrite({REG_LLIM, 0x4B, 0x00});
    f.wire.clearTraffic();
    f.sensor.setHighTemperatureLimitCelsius();
    f.checkSingleWrite({REG_HLIM, 0x50, 0x00});
  }

  TEST_CASE("limits outside of the range of the sensor are clamped") {
    Fixture f;
    f.begin(0x00);

    SUBCASE("just above the maximum") {
      f.sensor.setHighTemperatureLimitCelsius(128.0f);
      f.checkSingleWrite({REG_HLIM, 0x7F, 0xF0});
    }

    SUBCASE("far above the maximum") {
      f.sensor.setHighTemperatureLimitCelsius(1000.0f);
      f.checkSingleWrite({REG_HLIM, 0x7F, 0xF0});
    }

    SUBCASE("positive infinity") {
      f.sensor.setHighTemperatureLimitCelsius(std::numeric_limits<float>::infinity());
      f.checkSingleWrite({REG_HLIM, 0x7F, 0xF0});
    }

    SUBCASE("just below the minimum") {
      f.sensor.setLowTemperatureLimitCelsius(-128.0625f);
      f.checkSingleWrite({REG_LLIM, 0x80, 0x00});
    }

    SUBCASE("far below the minimum") {
      f.sensor.setLowTemperatureLimitCelsius(-1000.0f);
      f.checkSingleWrite({REG_LLIM, 0x80, 0x00});
    }

    SUBCASE("negative infinity") {
      f.sensor.setLowTemperatureLimitCelsius(-std::numeric_limits<float>::infinity());
      f.checkSingleWrite({REG_LLIM, 0x80, 0x00});
    }

    SUBCASE("not a number disables the limit") {
      f.sensor.setLowTemperatureLimitCelsius(std::nanf(""));
      f.checkSingleWrite({REG_LLIM, 0x80, 0x00});
    }
  }

  TEST_CASE("the limit registers do not touch the cached configuration") {
    Fixture f;
    f.begin(CFG_TM);
    f.sensor.setLowTemperatureLimitCelsius(10.0f);
    f.sensor.setHighTemperatureLimitCelsius(20.0f);
    f.wire.clearTraffic();
    f.sensor.setAlertPolarity(true);
    f.checkSingleWrite({REG_CFGR, (uint8_t)(CFG_TM | CFG_POL)});
  }
}

TEST_SUITE("device id") {
  TEST_CASE("getDeviceId() returns the content of the DIEID register") {
    Fixture f;
    f.begin(0x00);
    f.wire.queueResponse({0x75, 0x00});
    CHECK(f.sensor.getDeviceId() == 0x7500);
    f.checkSingleRead(REG_DIEID);
  }
}
