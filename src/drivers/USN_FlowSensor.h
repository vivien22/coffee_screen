#ifndef USN_FLOWSENSOR_H
#define USN_FLOWSENSOR_H

#include <Arduino.h>

// Two pulse flow sensors. The pulses are counted in an interrupt:
//   Nano R3 (ATmega328P): the pin change interrupt PCINT1 (port C, A0..A5), which this class owns;
//   Nano R4 (RA4M1): one external interrupt per input (attachInterrupt); only some pins have one: A1 and A2 do, A0 does not.
// The inputs (PIN_FLOW_IN, PIN_FLOW_OUT), the calibration (FLOW_K_FACTOR_IN / OUT, pulses per millilitre) and
// FLOW_UPDATE_MS are in Config.h. Only one instance is supported (the interrupt handlers use a single instance).
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
    float getInflowRate() { return lastInRate_; }
    float getOutflowRate() { return lastOutRate_; }

    // Accumulated volumes are returned in mL.
    float getVolumeIn() { return volumeIn_; }
    float getVolumeOut() { return volumeOut_; }
    float getNetVolume() { return volumeIn_ - volumeOut_; }   // inflow minus outflow

    // Called by the interrupt handlers; do not call from application code.
    void handlePortCChange(uint8_t currentPort);   // Nano R3: the whole of port C at once
    void countPulseIn()  { ++pulsesIn_; }          // Nano R4: one handler per input
    void countPulseOut() { ++pulsesOut_; }

    // Used by the interrupt handler to access the single active instance.
    static USN_FlowSensor* instance_;

private:
    // Modified by the ISR and copied/reset atomically in update().
    // 16 bits are enough: the counters are emptied every FLOW_UPDATE_MS.
    volatile uint16_t pulsesIn_;
    volatile uint16_t pulsesOut_;
#ifdef ARDUINO_ARCH_AVR
    volatile uint8_t lastPortC_;   // the state of port C at the last interrupt (the edges are found by comparing)
#endif

    uint32_t lastUpdateAt_;

    float volumeIn_;
    float volumeOut_;
    float lastInRate_;
    float lastOutRate_;
};

#endif
