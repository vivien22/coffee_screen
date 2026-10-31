#ifndef USN_FLOWSENSOR_H
#define USN_FLOWSENSOR_H

#include <Arduino.h>

// ATmega328P (Arduino Nano classique) : version PCINT pour A0/A1.
// Une seule instance de cette classe est prise en charge.
class USN_FlowSensor {
public:
    USN_FlowSensor(
        uint8_t inflowPin,
        uint8_t outflowPin,
        float kFactorIn = 12.0f,
        float kFactorOut = 50.0f
    );

    void begin();
    void update();
    void resetVolume();

    float getInflowRate();   // mL/min
    float getOutflowRate();  // mL/min
    float getNetFlowRate();  // mL/min
    float getNetVolume();    // mL

    // Appelée depuis ISR(PCINT1_vect). Ne pas appeler depuis loop().
    void handlePortCChange(uint8_t currentPort);

    // Exposée pour permettre à la routine ISR d'accéder à l'instance.
    static USN_FlowSensor* _instance;

private:
    uint8_t _inflowPin;
    uint8_t _outflowPin;

    uint8_t _inflowBit;
    uint8_t _outflowBit;

    float _kFactorIn;
    float _kFactorOut;

    volatile uint32_t _pulsesIn;
    volatile uint32_t _pulsesOut;

    volatile uint8_t _lastPortC;

    uint32_t _lastTime;
    float _netVolume;

    static float _lastInRate;
    static float _lastOutRate;
};

#endif
