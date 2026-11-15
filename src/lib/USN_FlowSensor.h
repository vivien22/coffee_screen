#ifndef USN_FLOWSENSOR_H
#define USN_FLOWSENSOR_H

#include <Arduino.h>

// Two pulse flow sensors on the classic ATmega328P Nano/Uno, counted by PCINT1.
// The inputs (PIN_FLOW_IN, PIN_FLOW_OUT: two different pins of A0..A5), the calibration
// (FLOW_K_FACTOR_IN / OUT, pulses per millilitre) and FLOW_UPDATE_MS are in config.h.
// Only one instance is supported because this library owns PCINT1_vect.
class USN_FlowSensor {
public:
    USN_FlowSensor();

    // Configure the pins and enable pin-change interrupts.
    void begin();

    // Call on every loop pass. Calculations are rate-limited internally.
    void update();

    // Start a new extraction (reset accumulated values) or stop rate reporting.
    void startExtraction();
    void stopExtraction();

    // Clear pulse counters, measured rates and accumulated volumes.
    void resetVolume();

    // Flow rates are returned in mL/min.
    float getInflowRate() { return _lastInRate; }
    float getOutflowRate() { return _lastOutRate; }

    // Accumulated volumes are returned in mL.
    float getVolumeIn() { return _volumeIn; }
    float getVolumeOut() { return _volumeOut; }
    float getNetVolume() { return _volumeIn - _volumeOut; }   // inflow minus outflow

    // Called by the PCINT1 interrupt handler; do not call from application code.
    void handlePortCChange(uint8_t currentPort);

    // Used by the interrupt handler to access the single active instance.
    static USN_FlowSensor* _instance;

private:
    // Modified by the ISR and copied/reset atomically in update().
    // 16 bits are enough: the counters are emptied every FLOW_UPDATE_MS.
    volatile uint16_t _pulsesIn;
    volatile uint16_t _pulsesOut;
    volatile uint8_t _lastPortC;

    uint32_t _lastUpdateAt;

    float _volumeIn;
    float _volumeOut;
    float _lastInRate;
    float _lastOutRate;
};

#endif
