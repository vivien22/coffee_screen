#include "temperature_sensor.h"
#include "config.h"
#include "debug.h"
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

// Callendar-Van Dusen coefficients of a platinum RTD (valid above 0 degrees C)
static const float RTD_A = 3.9083e-3f;
static const float RTD_B = -5.775e-7f;
static const float Z1 = -RTD_A;
static const float Z2 = RTD_A * RTD_A - 4.0f * RTD_B;
static const float Z3 = 4.0f * RTD_B / (float)RNOMINAL;
static const float Z4 = 2.0f * RTD_B;

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
  const float raw = (sqrtf(Z2 + Z3 * resistance) + Z1) / Z4;

  if (isnan(raw) || raw <= VALID_TEMPERATURE_MIN || raw > VALID_TEMPERATURE_MAX) {
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
