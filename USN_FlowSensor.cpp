#include "USN_FlowSensor.h"

// This implementation directly uses the ATmega328P port-C pin-change interrupt.
// It is intended for the classic Arduino Nano / Uno, not newer Nano variants.
#if !defined(__AVR_ATmega328P__)
#error "USN_FlowSensor PCINT1 implementation requires an ATmega328P (classic Nano)."
#endif

namespace {
  // Cumulative simulated output volume (mL), sampled at one-second intervals.
  // Cumulative simulated output volume (mL), sampled at one-second intervals.
  // Keep this table cumulative: the flow rate is derived from the difference
  // between two consecutive entries. The values reproduce the original test profile.
  const uint8_t SIMULATED_VOLUME_TABLE[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x03, 0x07, 0x0A, 0x0F, 0x10, 0x14, 0x1A, 0x20, 0x29,
    0x2F, 0x33, 0x3A, 0x40, 0x4A, 0x50, 0x5A, 0x60, 0x6A, 0x70
  };
  const uint8_t SIMULATED_VOLUME_TABLE_SIZE =
    sizeof(SIMULATED_VOLUME_TABLE) / sizeof(SIMULATED_VOLUME_TABLE[0]);
}

USN_FlowSensor* USN_FlowSensor::_instance = nullptr;
float USN_FlowSensor::_lastInRate = 0.0f;
float USN_FlowSensor::_lastOutRate = 0.0f;

// Static instance pointer is used by the ISR to forward pin changes to the object.
// The cached rates are static because only one instance is supported.
USN_FlowSensor::USN_FlowSensor() :
    _inflowBit(0),
    _outflowBit(1),
    _kFactorIn(USN_FLOW_SENSOR_K_FACTOR_IN),
    _kFactorOut(USN_FLOW_SENSOR_K_FACTOR_OUT),
    _pulsesIn(0),
    _pulsesOut(0),
    _lastPortC(0),
    _lastUpdateAt(0),
    _extractionStartedAt(0),
    _netVolume(0.0f),
    _outflowVolume(0.0f),
    _simulatedVolume(0.0f),
    _extractionActive(false)
{
}

// Initialize hardware acquisition. PCINT1 covers the whole PORTC group (A0..A5),
// while PCMSK1 enables only the two pins selected above.
void USN_FlowSensor::begin()
{
#if !USN_FLOW_SENSOR_USE_SIMULATION
    if (USN_FLOW_SENSOR_IN_PIN < A0 || USN_FLOW_SENSOR_IN_PIN > A5 ||
        USN_FLOW_SENSOR_OUT_PIN < A0 || USN_FLOW_SENSOR_OUT_PIN > A5 ||
        USN_FLOW_SENSOR_IN_PIN == USN_FLOW_SENSOR_OUT_PIN ||
        _kFactorIn <= 0.0f || _kFactorOut <= 0.0f) {
        return;
    }

    _inflowBit = USN_FLOW_SENSOR_IN_PIN - A0;
    _outflowBit = USN_FLOW_SENSOR_OUT_PIN - A0;

    pinMode(USN_FLOW_SENSOR_IN_PIN, INPUT_PULLUP);
    pinMode(USN_FLOW_SENSOR_OUT_PIN, INPUT_PULLUP);

    _instance = this;

    // Synchronize the cached port state before enabling the interrupt, so the
    // first interrupt only counts an edge that occurs after initialization.
    noInterrupts();
    _lastPortC = PINC;
    PCICR |= _BV(PCIE1);
    PCMSK1 |= _BV(_inflowBit);
    PCMSK1 |= _BV(_outflowBit);
    interrupts();
#endif

    _lastUpdateAt = millis();
}

// Pin-change interrupt for PORTC (A0..A5). Keep this routine short: it only
// forwards the current port state so pulse counting does not block loop().
ISR(PCINT1_vect)
{
#if !USN_FLOW_SENSOR_USE_SIMULATION
    if (USN_FlowSensor::_instance != nullptr) {
        USN_FlowSensor::_instance->handlePortCChange(PINC);
    }
#endif
}

// Count falling edges only (HIGH -> LOW). This matches the pulse output used
// by the flow sensors with the inputs configured using INPUT_PULLUP.
void USN_FlowSensor::handlePortCChange(uint8_t currentPort)
{
#if !USN_FLOW_SENSOR_USE_SIMULATION
    const uint8_t fallingEdges = _lastPortC & (uint8_t)~currentPort;

    if (fallingEdges & _BV(_inflowBit)) {
        ++_pulsesIn;
    }
    if (fallingEdges & _BV(_outflowBit)) {
        ++_pulsesOut;
    }

    _lastPortC = currentPort;
#else
    (void)currentPort;
#endif
}

// Public polling function. Call it on every loop pass; calculations are internally
// limited to USN_FLOW_SENSOR_UPDATE_INTERVAL_MS to avoid unnecessary work.
void USN_FlowSensor::update()
{
    const uint32_t now = millis();

    if ((uint32_t)(now - _lastUpdateAt) <
        USN_FLOW_SENSOR_UPDATE_INTERVAL_MS) {
        return;
    }

    _lastUpdateAt = now;

#if USN_FLOW_SENSOR_USE_SIMULATION
    updateSimulation(now);
#else
    updatePhysical();
#endif
}

// Convert pulse counts accumulated by the ISR into volumes and flow rates.
// Interrupts are briefly disabled only while the counters are copied and cleared.
void USN_FlowSensor::updatePhysical()
{
    const uint32_t currentTime = millis();
    const uint32_t duration = currentTime - _lastUpdateAt +
                              USN_FLOW_SENSOR_UPDATE_INTERVAL_MS;

    if (duration == 0) {
        return;
    }

    uint32_t pulsesIn;
    uint32_t pulsesOut;

    noInterrupts();
    pulsesIn = _pulsesIn;
    pulsesOut = _pulsesOut;
    _pulsesIn = 0;
    _pulsesOut = 0;
    interrupts();

    // Convert pulses to millilitres using each sensor's calibration factor, then
    // divide by the sampling duration to obtain a rate in mL/min.
    const float seconds = duration / 1000.0f;
    const float volumeIn = pulsesIn / _kFactorIn;
    const float volumeOut = pulsesOut / _kFactorOut;

    _netVolume += volumeIn - volumeOut;
    _outflowVolume += volumeOut;
    _lastInRate = (volumeIn / seconds) * 60.0f;
    _lastOutRate = (volumeOut / seconds) * 60.0f;
}

// Return the cumulative simulated volume at an arbitrary time. Linear interpolation
// between adjacent one-second samples avoids visible jumps in the displayed volume.
float USN_FlowSensor::getSimulatedVolumeAt(uint32_t elapsedMs)
{
    const uint32_t wholeSeconds = elapsedMs / 1000UL;
    const uint16_t fractionMs = elapsedMs % 1000UL;

    if (wholeSeconds >= (uint32_t)(SIMULATED_VOLUME_TABLE_SIZE - 1)) {
        return (float)SIMULATED_VOLUME_TABLE[SIMULATED_VOLUME_TABLE_SIZE - 1];
    }

    const float startVolume = SIMULATED_VOLUME_TABLE[wholeSeconds];
    const float endVolume = SIMULATED_VOLUME_TABLE[wholeSeconds + 1];
    return startVolume + (endVolume - startVolume) * (fractionMs / 1000.0f);
}

// Return the current one-second segment's volume increment, which is numerically
// equal to the average flow in mL/s for that segment.
float USN_FlowSensor::getSimulatedFlowRateAt(uint32_t elapsedMs)
{
    const uint32_t wholeSeconds = elapsedMs / 1000UL;

    if (wholeSeconds >= (uint32_t)(SIMULATED_VOLUME_TABLE_SIZE - 1)) {
        return 0.0f;
    }

    return (float)(
        SIMULATED_VOLUME_TABLE[wholeSeconds + 1] -
        SIMULATED_VOLUME_TABLE[wholeSeconds]
    );
}

// Update the simulated sensor values using elapsed time since extraction start.
// The same simulated profile feeds both the display and the temperature feedforward.
void USN_FlowSensor::updateSimulation(uint32_t now)
{
    if (!_extractionActive) {
        _simulatedVolume = 0.0f;
        _lastInRate = 0.0f;
        _lastOutRate = 0.0f;
        return;
    }

    const uint32_t elapsedMs = now - _extractionStartedAt;
    _simulatedVolume = getSimulatedVolumeAt(elapsedMs);
    _lastOutRate = getSimulatedFlowRateAt(elapsedMs) * 60.0f;
    _lastInRate = _lastOutRate;
    _outflowVolume = _simulatedVolume;
    _netVolume = 0.0f;
}

// Start a new extraction and restart the simulation timeline at zero.
void USN_FlowSensor::startExtraction()
{
    resetVolume();
    _extractionStartedAt = millis();
    _extractionActive = true;
}

// Stop the simulation clock and clear the instantaneous rates. Accumulated volume
// remains available so the final extraction volume can still be displayed.
void USN_FlowSensor::stopExtraction()
{
    _extractionActive = false;
    _lastInRate = 0.0f;
    _lastOutRate = 0.0f;
}

// Reset all counters and volume accumulators. Pulse counters are volatile because
// they are also modified by the pin-change ISR, so they must be cleared atomically.
void USN_FlowSensor::resetVolume()
{
    noInterrupts();
    _pulsesIn = 0;
    _pulsesOut = 0;
    interrupts();

    _lastUpdateAt = millis();
    _netVolume = 0.0f;
    _outflowVolume = 0.0f;
    _simulatedVolume = 0.0f;
    _lastInRate = 0.0f;
    _lastOutRate = 0.0f;
}

float USN_FlowSensor::getInflowRate() { return _lastInRate; }
float USN_FlowSensor::getOutflowRate() { return _lastOutRate; }
float USN_FlowSensor::getNetFlowRate() { return _lastInRate - _lastOutRate; }
float USN_FlowSensor::getNetVolume() { return _netVolume; }

float USN_FlowSensor::getOutflowVolume()
{
#if USN_FLOW_SENSOR_USE_SIMULATION
    return _simulatedVolume;
#else
    return _outflowVolume;
#endif
}
