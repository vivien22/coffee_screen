#include "temperature_sensor.h"
#include "../config.h"
#include "../debug.h"
#include <SPI.h>

// ---------- MAX31865 registers and bits ----------
static const uint8_t REG_CONFIG       = 0x00;
static const uint8_t REG_RTD_MSB      = 0x01;   // RTD value: 15 bits + fault flag (bit 0 of the LSB)
static const uint8_t REG_FAULT_STATUS = 0x07;
static const uint8_t WRITE_FLAG       = 0x80;

static const uint8_t CONFIG_BIAS         = 0x80;
static const uint8_t CONFIG_AUTO_CONVERT = 0x40;   // continuous conversion
static const uint8_t CONFIG_3WIRE        = 0x10;
static const uint8_t CONFIG_FAULT_CLEAR  = 0x02;

// Temperature of a platinum RTD from its resistance: polynomial of degree 2 fitted on the
// Callendar-Van Dusen equation between 0 and 200 degrees C (it replaces the exact formula and its
// square root, which costs flash). With x = 100 * (R / RNOMINAL - 1), in "ohms of a PT100 above 0 degrees C":
//     T = RTD_C2 * x * x + RTD_C1 * x + RTD_C0
// Error against the exact formula: 0.019 degrees C at most between 0 and 200 degrees C (0.002 around 93),
// below the resolution of the MAX31865 (0.034 degrees C per step with RREF = 428). Not valid outside
// 0..200 degrees C: those values are rejected anyway (VALID_TEMPERATURE_MIN / MAX).
static const float RTD_C2 = 1.05828606e-3f;
static const float RTD_C1 = 2.55580861f;
static const float RTD_C0 = 1.83700867e-2f;

// ---------- SPI access (mode 1: the screen uses mode 0, so every access is a transaction) ----------
static void writeRegister(uint8_t csPin, uint8_t reg, uint8_t value) {
  SPI.beginTransaction(SPISettings(MAX31865_SPI_HZ, MSBFIRST, SPI_MODE1));
  digitalWrite(csPin, LOW);
  SPI.transfer(reg | WRITE_FLAG);
  SPI.transfer(value);
  digitalWrite(csPin, HIGH);
  SPI.endTransaction();
}

static uint8_t readRegister8(uint8_t csPin, uint8_t reg) {
  SPI.beginTransaction(SPISettings(MAX31865_SPI_HZ, MSBFIRST, SPI_MODE1));
  digitalWrite(csPin, LOW);
  SPI.transfer(reg);
  const uint8_t value = SPI.transfer(0);
  digitalWrite(csPin, HIGH);
  SPI.endTransaction();
  return value;
}

static uint16_t readRegister16(uint8_t csPin, uint8_t reg) {
  SPI.beginTransaction(SPISettings(MAX31865_SPI_HZ, MSBFIRST, SPI_MODE1));
  digitalWrite(csPin, LOW);
  SPI.transfer(reg);
  const uint8_t high = SPI.transfer(0);
  const uint8_t low = SPI.transfer(0);
  digitalWrite(csPin, HIGH);
  SPI.endTransaction();
  return ((uint16_t)high << 8) | low;
}

static uint8_t baseConfig() {
  return CONFIG_BIAS | CONFIG_AUTO_CONVERT | (RTD_THREE_WIRE ? CONFIG_3WIRE : 0);
}

// ------------------------------------------------------------

TemperatureSensor::TemperatureSensor(uint8_t csPin) :
  csPin_(csPin),
  valid_(false),
  filterInitialized_(false),
  filtered_(0.0),
  lastFilterAt_(0),
  lastSampleAt_(0)
{
}

void TemperatureSensor::begin() {
  pinMode(csPin_, OUTPUT);
  digitalWrite(csPin_, HIGH);
  SPI.begin();

  writeRegister(csPin_, REG_CONFIG, baseConfig() | CONFIG_FAULT_CLEAR);
  delay(100);   // bias settling + first conversion (setup only)
  sample(millis());
}

void TemperatureSensor::update(uint32_t now) {
  if ((uint32_t)(now - lastSampleAt_) < SENSOR_PERIOD_MS) {
    return;
  }
  sample(now);
}

void TemperatureSensor::sample(uint32_t now) {
  lastSampleAt_ = now;

  const uint16_t rtdRegister = readRegister16(csPin_, REG_RTD_MSB);

  if (rtdRegister & 0x01) {   // fault flag
    const uint8_t fault = readRegister8(csPin_, REG_FAULT_STATUS);
    writeRegister(csPin_, REG_CONFIG, baseConfig() | CONFIG_FAULT_CLEAR);
    valid_ = false;
    (void)fault;   // only used by the debug trace
    DEBUG_PRINT(F("MAX31865 fault: 0x"));
    DEBUG_PRINTLN(fault, HEX);
    return;
  }

  const float resistance = (float)(rtdRegister >> 1) / 32768.0f * (float)RREF;
  const float x = resistance * (100.0f / (float)RNOMINAL) - 100.0f;
  const float raw = (RTD_C2 * x + RTD_C1) * x + RTD_C0;

  if (raw <= VALID_TEMPERATURE_MIN || raw > VALID_TEMPERATURE_MAX) {
    valid_ = false;
    return;
  }

  filtered_ = filter(raw, now);
  valid_ = true;
}

// First-order low-pass filter, time constant TEMP_FILTER_TIME_CONSTANT.
double TemperatureSensor::filter(double raw, uint32_t now) {
  if (!filterInitialized_) {
    filterInitialized_ = true;
    lastFilterAt_ = now;
    return raw;
  }

  const double dt = (now - lastFilterAt_) / 1000.0;
  lastFilterAt_ = now;
  if (dt <= 0.0) {
    return filtered_;
  }

  const double alpha = dt / (TEMP_FILTER_TIME_CONSTANT + dt);
  return filtered_ + alpha * (raw - filtered_);
}
