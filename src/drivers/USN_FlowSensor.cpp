// USN_FlowSensor.cpp: pulse counting of the two flow sensors (pin change interrupt), volumes and flow rate.
#include "../Config.h"

// Not compiled when the flow simulator replaces the sensors (see FLOW_SIMULATOR in Config.h)
#ifndef FLOW_SIMULATOR

#include "USN_FlowSensor.h"

#include <avr/interrupt.h>
#include <avr/io.h>

// This implementation is specifically for the classic Arduino Nano/Uno.
#if !defined(__AVR_ATmega328P__)
#error "USN_FlowSensor requires an ATmega328P (classic Nano/Uno)."
#endif

USN_FlowSensor* USN_FlowSensor::instance_ = nullptr;

// Bits of PORTC (A0..A5) of the two inputs
#define FLOW_IN_BIT   (PIN_FLOW_IN - A0)
#define FLOW_OUT_BIT  (PIN_FLOW_OUT - A0)

// Counts the pulses of the two flow sensors in a pin change interrupt, and turns them into volumes and flow rate.
USN_FlowSensor::USN_FlowSensor() :
    pulsesIn_(0),
    pulsesOut_(0),
    lastPortC_(0),
    lastUpdateAt_(0),
    volumeIn_(0.0f),
    volumeOut_(0.0f),
    lastInRate_(0.0f),
    lastOutRate_(0.0f)
{
}

// Configure the In input (and the Out input if FLOW_OUT_SENSOR) and enable PCINT1 for them.
void USN_FlowSensor::begin()
{
    pinMode(PIN_FLOW_IN, INPUT_PULLUP);
#ifdef FLOW_OUT_SENSOR
    pinMode(PIN_FLOW_OUT, INPUT_PULLUP);
#endif

    instance_ = this;

    // Synchronize the port state before enabling the interrupt.
    noInterrupts();
    lastPortC_ = PINC;
    PCMSK1 |= _BV(FLOW_IN_BIT);
#ifdef FLOW_OUT_SENSOR
    PCMSK1 |= _BV(FLOW_OUT_BIT);
#endif
    PCIFR |= _BV(PCIF1);  // Clear any pending PORTC pin-change flag.
    PCICR |= _BV(PCIE1);
    interrupts();

    lastUpdateAt_ = millis();
}

// PORTC pin-change interrupt (A0..A5).
ISR(PCINT1_vect)
{
    if (USN_FlowSensor::instance_ != nullptr) {
        USN_FlowSensor::instance_->handlePortCChange(PINC);
    }
}

// Count falling edges (HIGH -> LOW), as used with INPUT_PULLUP.
void USN_FlowSensor::handlePortCChange(uint8_t currentPort)
{
    const uint8_t fallingEdges = lastPortC_ & (uint8_t)~currentPort;

    if (fallingEdges & _BV(FLOW_IN_BIT)) {
        ++pulsesIn_;
    }

#ifdef FLOW_OUT_SENSOR
    if (fallingEdges & _BV(FLOW_OUT_BIT)) {
        ++pulsesOut_;
    }
#endif

    lastPortC_ = currentPort;
}

// Call from loop(): every FLOW_UPDATE_MS, the pulses counted since the last time are converted
// into volumes and flow rates (the actual elapsed time is used for the rates).
void USN_FlowSensor::update()
{
    const uint32_t now = millis();
    const uint32_t durationMs = now - lastUpdateAt_;

    if (durationMs < FLOW_UPDATE_MS) {
        return;
    }
    lastUpdateAt_ = now;

    noInterrupts();
    const uint16_t pulsesIn = pulsesIn_;
    const uint16_t pulsesOut = pulsesOut_;
    pulsesIn_ = 0;
    pulsesOut_ = 0;
    interrupts();

    const float minutes = durationMs / 60000.0f;
    const float volumeIn = pulsesIn / FLOW_K_FACTOR_IN;
    const float volumeOut = pulsesOut / FLOW_K_FACTOR_OUT;
    volumeIn_ += volumeIn;
    volumeOut_ += volumeOut;
    lastInRate_ = volumeIn / minutes;
    lastOutRate_ = volumeOut / minutes;
}

// Begin a new extraction and clear previous measurements.
void USN_FlowSensor::startExtraction()
{
    resetVolume();
}

// Stop reporting instantaneous flow. Accumulated volumes are preserved.
void USN_FlowSensor::stopExtraction()
{
    lastInRate_ = 0.0f;
    lastOutRate_ = 0.0f;
}

// Reset pulse counters and accumulated measurements.
void USN_FlowSensor::resetVolume()
{
    noInterrupts();
    pulsesIn_ = 0;
    pulsesOut_ = 0;
    interrupts();

    lastUpdateAt_ = millis();
    volumeIn_ = 0.0f;
    volumeOut_ = 0.0f;
    lastInRate_ = 0.0f;
    lastOutRate_ = 0.0f;
}

#endif  // FLOW_SIMULATOR
