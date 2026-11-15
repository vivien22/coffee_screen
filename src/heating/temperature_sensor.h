/*
  temperature_sensor: MAX31865 reading every SENSOR_PERIOD_MS, validity check
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

    // False after a sensor fault or an implausible value: the heater must stay off.
    bool isValid() const { return valid_; }

    // Filtered temperature (degrees C), meaningful when isValid().
    double celsius() const { return filtered_; }

  private:
    void sample(uint32_t now);
    double filter(double raw, uint32_t now);

    uint8_t csPin_;
    bool valid_;
    bool filterInitialized_;
    double filtered_;
    uint32_t lastFilterAt_;
    uint32_t lastSampleAt_;
};
