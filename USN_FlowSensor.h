#ifndef USN_FLOWSENSOR_H
#define USN_FLOWSENSOR_H

#include <Arduino.h>

// Minimum interval between flow calculations.
#define USN_FLOW_SENSOR_UPDATE_INTERVAL_MS   250UL

// Inputs must be analog pins A0..A5 on the classic ATmega328P Nano/Uno.
#define USN_FLOW_SENSOR_IN_PIN               A0
#define USN_FLOW_SENSOR_OUT_PIN              A1

// Calibration factors in pulses per millilitre.
#define USN_FLOW_SENSOR_K_FACTOR_IN          12.56f
#define USN_FLOW_SENSOR_K_FACTOR_OUT         50.0f

// ATmega328P implementation using PCINT1 (A0..A5).
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
    float getInflowRate();
    float getOutflowRate();
    float getNetFlowRate();

    // Accumulated volumes are returned in mL.
    float getVolumeIn();
    float getVolumeOut();
    float getNetVolume();       // inflow minus outflow (kept for compatibility)
    float getOutflowVolume();   // equivalent to getVolumeOut() (kept for compatibility)

    // Called by the PCINT1 interrupt handler; do not call from application code.
    void handlePortCChange(uint8_t currentPort);

    // Used by the interrupt handler to access the single active instance.
    static USN_FlowSensor* _instance;

private:
    void updatePhysical(uint32_t durationMs);

    uint8_t _inflowBit;
    uint8_t _outflowBit;

    float _kFactorIn;
    float _kFactorOut;

    // Modified by the ISR and copied/reset atomically in update().
    volatile uint32_t _pulsesIn;
    volatile uint32_t _pulsesOut;
    volatile uint8_t _lastPortC;

    uint32_t _lastUpdateAt;

    float _volumeIn;
    float _volumeOut;
    float _netVolume;
    float _outflowVolume;
    float _lastInRate;
    float _lastOutRate;

    bool _extractionActive;
};

#endif
