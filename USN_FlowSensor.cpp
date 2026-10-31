#include "USN_FlowSensor.h"

#if !defined(__AVR_ATmega328P__)
#error "USN_FlowSensor A0/A1 PCINT version: cible prevue = ATmega328P (Nano classique)."
#endif

USN_FlowSensor* USN_FlowSensor::_instance = nullptr;

float USN_FlowSensor::_lastInRate = 0.0f;
float USN_FlowSensor::_lastOutRate = 0.0f;

USN_FlowSensor::USN_FlowSensor(
    uint8_t inflowPin,
    uint8_t outflowPin,
    float kFactorIn,
    float kFactorOut
) :
    _inflowPin(inflowPin),
    _outflowPin(outflowPin),
    _inflowBit(0),
    _outflowBit(0),
    _kFactorIn(kFactorIn),
    _kFactorOut(kFactorOut),
    _pulsesIn(0),
    _pulsesOut(0),
    _lastPortC(0),
    _lastTime(0),
    _netVolume(0.0f)
{
}

void USN_FlowSensor::begin()
{
    // A0..A5 correspondent aux bits PC0..PC5 du port C.
    if (_inflowPin < A0 || _inflowPin > A5 ||
        _outflowPin < A0 || _outflowPin > A5 ||
        _inflowPin == _outflowPin ||
        _kFactorIn <= 0.0f || _kFactorOut <= 0.0f) {
        return;
    }

    _inflowBit = _inflowPin - A0;
    _outflowBit = _outflowPin - A0;

    pinMode(_inflowPin, INPUT_PULLUP);
    pinMode(_outflowPin, INPUT_PULLUP);

    // Cette bibliothèque est conçue pour une seule instance.
    _instance = this;

    noInterrupts();

    // Mémorise l'état initial pour ne compter que les fronts descendants.
    _lastPortC = PINC;

    // Active le groupe PCINT1 (port C : A0 à A5).
    PCICR |= _BV(PCIE1);

    // Active uniquement les deux broches choisies dans ce groupe.
    PCMSK1 |= _BV(_inflowBit);
    PCMSK1 |= _BV(_outflowBit);

    interrupts();

    _lastTime = millis();
}

ISR(PCINT1_vect)
{
    if (USN_FlowSensor::_instance != nullptr) {
        USN_FlowSensor::_instance->handlePortCChange(PINC);
    }
}

void USN_FlowSensor::handlePortCChange(uint8_t currentPort)
{
    // Front descendant uniquement : entrée précédente HIGH, actuelle LOW.
    const uint8_t fallingEdges = _lastPortC & (uint8_t)~currentPort;

    if (fallingEdges & _BV(_inflowBit)) {
        ++_pulsesIn;
    }

    if (fallingEdges & _BV(_outflowBit)) {
        ++_pulsesOut;
    }

    _lastPortC = currentPort;
}

void USN_FlowSensor::update()
{
    const uint32_t currentTime = millis();
    const uint32_t duration = currentTime - _lastTime;

    // Fenêtre de mesure de 200 ms minimum.
    if (duration < 200UL) {
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

    _lastTime = currentTime;

    const float seconds = duration / 1000.0f;
    const float volumeIn = pulsesIn / _kFactorIn;
    const float volumeOut = pulsesOut / _kFactorOut;

    _netVolume += volumeIn - volumeOut;

    _lastInRate = (volumeIn / seconds) * 60.0f;
    _lastOutRate = (volumeOut / seconds) * 60.0f;
}

void USN_FlowSensor::resetVolume()
{
    _netVolume = 0.0f;
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

float USN_FlowSensor::getNetVolume()
{
    return _netVolume;
}
