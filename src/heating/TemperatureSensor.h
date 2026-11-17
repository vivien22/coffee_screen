/*
  TemperatureSensor: MAX31865 reading every SENSOR_PERIOD_MS, validity check
  (sensor fault, implausible value) and low-pass filter.
  Minimal driver on the hardware SPI (shared with the screen, own chip select):
  the chip converts continuously, so reading it never blocks the loop.
  Knows nothing about the heater or the screen.
*/

#pragma once
#include <Arduino.h>

class TemperatureSensor {
  public:
    explicit TemperatureSensor(uint8_t csPin);

    // Configure the sensor and take a first sample.
    void begin();

    // Call on every loop pass; the sensor is read once per SENSOR_PERIOD_MS.
    void update(uint32_t now);

    // False after SENSOR_FAULT_TOLERANCE faulty readings in a row (sensor fault or implausible value): the heater
    // must stay off. One or two stray readings do not count: the last good temperature is kept meanwhile.
    bool isValid() const { return valid_; }

    // Filtered temperature (degrees C), meaningful when isValid().
    double celsius() const { return filtered_; }

  private:
    void sample(uint32_t now);
    void countBadReading();
    double filter(double raw, uint32_t now);

    uint8_t csPin_;
    bool valid_;
    uint8_t badReadings_;       // faulty readings in a row (fault flag or implausible value)
    bool filterInitialized_;
    double filtered_;
    uint32_t lastFilterAt_;
    uint32_t lastSampleAt_;
};
