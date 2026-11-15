#include "USN_FlowSensor.h"

#include <avr/interrupt.h>
#include <avr/io.h>

// This implementation is specifically for the classic Arduino Nano/Uno.
#if !defined(__AVR_ATmega328P__)
#error "USN_FlowSensor requires an ATmega328P (classic Nano/Uno)."
#endif

USN_FlowSensor* USN_FlowSensor::_instance = nullptr;

USN_FlowSensor::USN_FlowSensor() :
    _inflowBit(0),
    _outflowBit(1),
    _kFactorIn(USN_FLOW_SENSOR_K_FACTOR_IN),
    _kFactorOut(USN_FLOW_SENSOR_K_FACTOR_OUT),
    _pulsesIn(0),
    _pulsesOut(0),
    _lastPortC(0),
    _lastUpdateAt(0),
    _volumeIn(0.0f),
    _volumeOut(0.0f),
    _netVolume(0.0f),
    _outflowVolume(0.0f),
    _lastInRate(0.0f),
    _lastOutRate(0.0f)
{
}

// Configure the selected pins and enable PCINT1 for those two inputs.
void USN_FlowSensor::begin()
{
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

    // Synchronize the port state before enabling the interrupt.
    noInterrupts();
    _lastPortC = PINC;
    PCMSK1 |= _BV(_inflowBit) | _BV(_outflowBit);
    PCIFR |= _BV(PCIF1);  // Clear any pending PORTC pin-change flag.
    PCICR |= _BV(PCIE1);
    interrupts();

    _lastUpdateAt = millis();
}

// PORTC pin-change interrupt (A0..A5).
ISR(PCINT1_vect)
{
    if (USN_FlowSensor::_instance != nullptr) {
        USN_FlowSensor::_instance->handlePortCChange(PINC);
    }
}

// Count falling edges (HIGH -> LOW), as used with INPUT_PULLUP.
void USN_FlowSensor::handlePortCChange(uint8_t currentPort)
{
    const uint8_t fallingEdges = _lastPortC & (uint8_t)~currentPort;

    if (fallingEdges & _BV(_inflowBit)) {
        ++_pulsesIn;
    }

    if (fallingEdges & _BV(_outflowBit)) {
        ++_pulsesOut;
    }

    _lastPortC = currentPort;
}

// Call from loop(). The actual elapsed time is used for rate calculations.
void USN_FlowSensor::update()
{
    const uint32_t now = millis();
    const uint32_t duration = now - _lastUpdateAt;

    if (duration < USN_FLOW_SENSOR_UPDATE_INTERVAL_MS) {
        return;
    }

    _lastUpdateAt = now;
    updatePhysical(duration);
}

// Convert accumulated pulses into volumes and flow rates.
void USN_FlowSensor::updatePhysical(uint32_t durationMs)
{
    if (durationMs == 0) {
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

    const float seconds = durationMs / 1000.0f;
    const float volumeIn = pulsesIn / _kFactorIn;
    const float volumeOut = pulsesOut / _kFactorOut;
    _volumeIn += volumeIn;
    _volumeOut += volumeOut;

    // Keep the previous volume accessors working.
    _netVolume = _volumeIn - _volumeOut;
    _outflowVolume = _volumeOut;

    _lastInRate = (volumeIn / seconds) * 60.0f;
    _lastOutRate = (volumeOut / seconds) * 60.0f;
}

// Begin a new extraction and clear previous measurements.
void USN_FlowSensor::startExtraction()
{
    resetVolume();
}

// Stop reporting instantaneous flow. Accumulated volumes are preserved.
void USN_FlowSensor::stopExtraction()
{
    _lastInRate = 0.0f;
    _lastOutRate = 0.0f;
}

// Reset pulse counters and accumulated measurements.
void USN_FlowSensor::resetVolume()
{
    noInterrupts();
    _pulsesIn = 0;
    _pulsesOut = 0;
    interrupts();

    _lastUpdateAt = millis();
    _volumeIn = 0.0f;
    _volumeOut = 0.0f;
    _netVolume = 0.0f;
    _outflowVolume = 0.0f;
    _lastInRate = 0.0f;
    _lastOutRate = 0.0f;
}

float USN_FlowSensor::getInflowRate()
{
    return _lastInRate;
}

float USN_FlowSensor::getOutflowRate()
{
    return _lastOutRate;
}

float USN_FlowSensor::getNetFlowRate()
{
    return _lastInRate - _lastOutRate;
}

float USN_FlowSensor::getVolumeIn()
{
    return _volumeIn;
}

float USN_FlowSensor::getVolumeOut()
{
    return _volumeOut;
}

float USN_FlowSensor::getNetVolume()
{
    return _netVolume;
}

float USN_FlowSensor::getOutflowVolume()
{
    return _outflowVolume;
}
