#include "../config.h"

// Not compiled when the flow simulator replaces the sensors (see FLOW_SIMULATOR in config.h)
#ifndef FLOW_SIMULATOR

#include "USN_FlowSensor.h"

#include <avr/interrupt.h>
#include <avr/io.h>

// This implementation is specifically for the classic Arduino Nano/Uno.
#if !defined(__AVR_ATmega328P__)
#error "USN_FlowSensor requires an ATmega328P (classic Nano/Uno)."
#endif

USN_FlowSensor* USN_FlowSensor::_instance = nullptr;

// Bits of PORTC (A0..A5) of the two inputs
#define FLOW_IN_BIT   (PIN_FLOW_IN - A0)
#define FLOW_OUT_BIT  (PIN_FLOW_OUT - A0)

USN_FlowSensor::USN_FlowSensor() :
    _pulsesIn(0),
    _pulsesOut(0),
    _lastPortC(0),
    _lastUpdateAt(0),
    _volumeIn(0.0f),
    _volumeOut(0.0f),
    _lastInRate(0.0f),
    _lastOutRate(0.0f)
{
}

// Configure the two inputs and enable PCINT1 for them.
void USN_FlowSensor::begin()
{
    pinMode(PIN_FLOW_IN, INPUT_PULLUP);
    pinMode(PIN_FLOW_OUT, INPUT_PULLUP);

    _instance = this;

    // Synchronize the port state before enabling the interrupt.
    noInterrupts();
    _lastPortC = PINC;
    PCMSK1 |= _BV(FLOW_IN_BIT) | _BV(FLOW_OUT_BIT);
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

    if (fallingEdges & _BV(FLOW_IN_BIT)) {
        ++_pulsesIn;
    }

    if (fallingEdges & _BV(FLOW_OUT_BIT)) {
        ++_pulsesOut;
    }

    _lastPortC = currentPort;
}

// Call from loop(): every FLOW_UPDATE_MS, the pulses counted since the last time are converted
// into volumes and flow rates (the actual elapsed time is used for the rates).
void USN_FlowSensor::update()
{
    const uint32_t now = millis();
    const uint32_t durationMs = now - _lastUpdateAt;

    if (durationMs < FLOW_UPDATE_MS) {
        return;
    }
    _lastUpdateAt = now;

    noInterrupts();
    const uint16_t pulsesIn = _pulsesIn;
    const uint16_t pulsesOut = _pulsesOut;
    _pulsesIn = 0;
    _pulsesOut = 0;
    interrupts();

    const float minutes = durationMs / 60000.0f;
    const float volumeIn = pulsesIn / FLOW_K_FACTOR_IN;
    const float volumeOut = pulsesOut / FLOW_K_FACTOR_OUT;
    _volumeIn += volumeIn;
    _volumeOut += volumeOut;
    _lastInRate = volumeIn / minutes;
    _lastOutRate = volumeOut / minutes;
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
    _lastInRate = 0.0f;
    _lastOutRate = 0.0f;
}

#endif  // FLOW_SIMULATOR
