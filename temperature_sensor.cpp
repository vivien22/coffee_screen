#include "temperature_sensor.h"
#include "config.h"
#include "debug.h"

TemperatureSensor::TemperatureSensor(uint8_t csPin) :
  thermo_(csPin),
  valid_(false),
  filterInitialized_(false),
  filtered_(0.0),
  lastFilterAt_(0),
  lastSampleAt_(0)
{
}

void TemperatureSensor::begin() {
  thermo_.begin(RTD_WIRING);
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

  const uint8_t fault = thermo_.readFault();
  if (fault != 0) {
    thermo_.clearFault();
    valid_ = false;
    DEBUG_PRINT(F("MAX31865 fault: 0x"));
    DEBUG_PRINTLN(fault, HEX);
    return;
  }

  const double raw = thermo_.temperature(RNOMINAL, RREF);
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
