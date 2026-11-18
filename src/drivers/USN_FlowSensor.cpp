// USN_FlowSensor.cpp: pulse counting of the two flow sensors (pin change interrupt), volumes and flow rate.
#include "../Config.h"

// Not compiled when the flow simulator replaces the sensors (see FLOW_SIMULATOR in Config.h)
#ifndef FLOW_SIMULATOR

#include "USN_FlowSensor.h"

#if defined(ARDUINO_ARCH_AVR)
#include <avr/interrupt.h>
#include <avr/io.h>

#if !defined(__AVR_ATmega328P__)
#error "USN_FlowSensor on AVR requires an ATmega328P (classic Nano/Uno)."
#endif
#endif

USN_FlowSensor* USN_FlowSensor::instance_ = nullptr;

#ifdef ARDUINO_ARCH_AVR
// Bits of PORTC (A0..A5) of the two inputs
#define FLOW_IN_BIT   (PIN_FLOW_IN - A0)
#define FLOW_OUT_BIT  (PIN_FLOW_OUT - A0)
#endif

// Counts the pulses of the two flow sensors in a pin change interrupt, and turns them into volumes and flow rate.
USN_FlowSensor::USN_FlowSensor() :
    pulsesIn_(0),
    pulsesOut_(0),
#ifdef ARDUINO_ARCH_AVR
    lastPortC_(0),
#endif
    lastUpdateAt_(0),
    volumeIn_(0.0f),
    volumeOut_(0.0f),
    lastInRate_(0.0f),
    lastOutRate_(0.0f)
{
}

#ifdef ARDUINO_ARCH_AVR
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
#else
// Nano R4: one external interrupt per input, on the falling edge (the inputs have a pull-up). The pin must be set
// as an input BEFORE attachInterrupt() on this core. The pins need an interrupt channel (see PIN_FLOW_IN in Config.h).
static void flowInInterrupt()  { if (USN_FlowSensor::instance_ != nullptr) USN_FlowSensor::instance_->countPulseIn(); }
#ifdef FLOW_OUT_SENSOR
static void flowOutInterrupt() { if (USN_FlowSensor::instance_ != nullptr) USN_FlowSensor::instance_->countPulseOut(); }
#endif

void USN_FlowSensor::begin()
{
    pinMode(PIN_FLOW_IN, INPUT_PULLUP);
#ifdef FLOW_OUT_SENSOR
    pinMode(PIN_FLOW_OUT, INPUT_PULLUP);
#endif

    instance_ = this;

    attachInterrupt(digitalPinToInterrupt(PIN_FLOW_IN), flowInInterrupt, FALLING);
#ifdef FLOW_OUT_SENSOR
    attachInterrupt(digitalPinToInterrupt(PIN_FLOW_OUT), flowOutInterrupt, FALLING);
#endif

    lastUpdateAt_ = millis();
}
#endif

#ifdef ARDUINO_ARCH_AVR
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
#endif

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
    const uint16_t pulsesOut = pulsesOut_;   // 0 while the Out sensor is not wired
    pulsesIn_ = 0;
    pulsesOut_ = 0;
    interrupts();

    const float minutes = durationMs / 60000.0f;
    const float volumeIn = pulsesIn / FLOW_K_FACTOR_IN;
#ifdef FLOW_OUT_SENSOR
    const float volumeOut = pulsesOut / FLOW_K_FACTOR_OUT;
#else
    // The Out sensor is not wired: the return of the OPV valve is estimated as a part of the water that enters
    (void)pulsesOut;
    const float volumeOut = volumeIn * FLOW_OUT_ESTIMATE_RATIO;
#endif
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
