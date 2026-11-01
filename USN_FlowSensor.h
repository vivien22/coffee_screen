#ifndef USN_FLOWSENSOR_H
#define USN_FLOWSENSOR_H

#include <Arduino.h>

// Library configuration: change these values here, not in the application sketch.
// Set to 1 to use the built-in extraction profile; set to 0 for real sensors.
#define USN_FLOW_SENSOR_USE_SIMULATION       1

// Minimum time between two flow calculations. The sensor is still polled from loop(),
// but pulse counting itself is handled asynchronously by the pin-change interrupt.
#define USN_FLOW_SENSOR_UPDATE_INTERVAL_MS   250UL

// Both inputs must be analog pins A0..A5 on the classic ATmega328P Nano.
#define USN_FLOW_SENSOR_IN_PIN               A0
#define USN_FLOW_SENSOR_OUT_PIN              A1

// Calibration factors expressed in pulses per millilitre. Calibrate each sensor
// independently by collecting a known volume and counting the corresponding pulses.
#define USN_FLOW_SENSOR_K_FACTOR_IN          12.0f
#define USN_FLOW_SENSOR_K_FACTOR_OUT         50.0f

// ATmega328P / classic Arduino Nano implementation using PCINT1 (A0..A5).
// Only one instance is supported because the library owns PCINT1_vect.
class USN_FlowSensor {
public:
    USN_FlowSensor();

    // Configure the selected pins and enable PCINT1 when using physical sensors.
    // In simulation mode this only initializes the internal update timer.
    void begin();

    // Call from loop(); the library internally applies its configured period
    // and selects either physical acquisition or the built-in simulation.
    void update();

    // Extraction lifecycle. The library owns the simulation clock and counters.
    // Call when an extraction starts/stops. Starting an extraction clears the
    // accumulated volumes and starts the simulation profile from time zero.
    void startExtraction();
    void stopExtraction();

    // Clear pulse counters, measured rates and accumulated volumes.
    void resetVolume();

    // Flow rates are returned in mL/min. The net rate is inflow minus outflow.
    float getInflowRate();       // mL/min
    float getOutflowRate();      // mL/min
    float getNetFlowRate();      // mL/min
    float getNetVolume();        // mL (inflow minus outflow)
    float getOutflowVolume();    // mL

    // Simulation-profile accessors. The returned volume is interpolated between
    // one-second samples; the flow rate is the slope over the current second.
    // These static methods are also kept for compatibility with volume.cpp.
    static float getSimulatedVolumeAt(uint32_t elapsedMs);
    static float getSimulatedFlowRateAt(uint32_t elapsedMs); // mL/s

    // Called from ISR(PCINT1_vect), never from loop().
    void handlePortCChange(uint8_t currentPort);
    static USN_FlowSensor* _instance;

private:
    void updatePhysical();
    void updateSimulation(uint32_t now);

    // Bit positions in PINC corresponding to the configured analog input pins.
    uint8_t _inflowBit;
    uint8_t _outflowBit;

    float _kFactorIn;
    float _kFactorOut;

    // Updated inside the pin-change ISR and copied/reset atomically in updatePhysical().
    volatile uint32_t _pulsesIn;
    volatile uint32_t _pulsesOut;
    volatile uint8_t _lastPortC;

    // Timing state used to rate-limit calculations and measure extraction duration.
    uint32_t _lastUpdateAt;
    uint32_t _extractionStartedAt;

    // Accumulated physical/simulated volumes, in millilitres.
    float _netVolume;
    float _outflowVolume;
    float _simulatedVolume;

    bool _extractionActive;

    static float _lastInRate;
    static float _lastOutRate;
};

#endif
