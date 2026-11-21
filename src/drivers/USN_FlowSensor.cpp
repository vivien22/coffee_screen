// USN_FlowSensor.cpp: pulse counting of the two flow sensors, volumes and flow rate.
#include "../Config.h"

#include "USN_FlowSensor.h"
#include "../platform/Platform.h"

USN_FlowSensor* USN_FlowSensor::instance_ = nullptr;

// Counts the pulses of the two flow sensors (in the interrupt of the board, see platform/), and turns them into volumes
// and flow rate.
USN_FlowSensor::USN_FlowSensor() :
    pulsesIn_(0),
    pulsesOut_(0),
    lastUpdateAt_(0),
    volumeIn_(0.0f),
    volumeOut_(0.0f),
    lastInRate_(0.0f),
    lastOutRate_(0.0f)
{
}

// Configure the In and Out inputs and enable the interrupt of the board for them.
void USN_FlowSensor::begin()
{
    instance_ = this;
    platformPulsesBegin();
    lastUpdateAt_ = millis();
}

// Called by the interrupt of the board (platform/) on each falling edge of an input.
void flowPulseIn()
{
    if (USN_FlowSensor::instance_ != nullptr) {
        USN_FlowSensor::instance_->countPulseIn();
    }
}

void flowPulseOut()
{
    if (USN_FlowSensor::instance_ != nullptr) {
        USN_FlowSensor::instance_->countPulseOut();
    }
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

// In - Out including the pulses that update() has not converted yet (the pulses counted since it ran).
float USN_FlowSensor::getNetVolumeLive()
{
    noInterrupts();
    const uint16_t pulsesIn = pulsesIn_;
    const uint16_t pulsesOut = pulsesOut_;
    interrupts();

    const float in = pulsesIn / FLOW_K_FACTOR_IN;
    const float out = pulsesOut / FLOW_K_FACTOR_OUT;
    return (volumeIn_ + in) - (volumeOut_ + out);
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
