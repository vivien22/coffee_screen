#ifndef FLOWSENSOR_H
#define FLOWSENSOR_H

#include <Arduino.h>

// Two pulse flow sensors. The pulses are counted in an interrupt that depends on the board (platform/Platform.h: the pin
// change interrupt on the Nano R3, one external interrupt per input on the R4), which calls flowPulseIn() / flowPulseOut().
// The inputs (PIN_FLOW_IN, PIN_FLOW_OUT), the calibration (FLOW_K_FACTOR_IN / OUT, pulses per millilitre) and
// FLOW_UPDATE_MS are in Config.h. Only one instance is supported (the interrupt handlers use a single instance).
class FlowSensor {
public:
    FlowSensor();

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
    float getInflowRate() { return lastInRate_; }
    float getOutflowRate() { return lastOutRate_; }

    // Accumulated volumes are returned in mL.
    float getVolumeIn() { return volumeIn_; }
    float getVolumeOut() { return volumeOut_; }
    float getNetVolume() { return volumeIn_ - volumeOut_; }   // inflow minus outflow

    // The same at this very moment: the pulses counted since the last update() are added (that one is only up to date every
    // FLOW_UPDATE_MS). Used to stop the pump at the right volume.
    float getNetVolumeLive();

    // Called by flowPulseIn() / flowPulseOut() from the interrupt; do not call from application code.
    void countPulseIn()  { ++pulsesIn_; }
    void countPulseOut() { ++pulsesOut_; }

    // Used by the interrupt handler to access the single active instance.
    static FlowSensor* instance_;

private:
    // Modified by the ISR and copied/reset atomically in update().
    // 16 bits are enough: the counters are emptied every FLOW_UPDATE_MS.
    volatile uint16_t pulsesIn_;
    volatile uint16_t pulsesOut_;

    uint32_t lastUpdateAt_;

    float volumeIn_;
    float volumeOut_;
    float lastInRate_;
    float lastOutRate_;
};

#endif
