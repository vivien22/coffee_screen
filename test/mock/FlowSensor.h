// Test replacement of src/drivers/FlowSensor.h: the flow sensor values are set by the tests.
#pragma once
#include <Arduino.h>

class FlowSensor {
  public:
    float in = 0, out = 0, rate = 0;   // volumes In / Out (ml) and inflow rate (ml/min), set by the tests
    float &vol = in;                  // old name of In, used by some tests
    void begin() {}
    void update() {}
    void startExtraction() { in = 0; out = 0; }
    void stopExtraction() { rate = 0; }
    float outRate = 0;                // outflow rate (ml/min), set by the tests
    float getInflowRate() { return rate; }
    float getOutflowRate() { return outRate; }
    float getNetVolume() { return in - out; }
    float getNetVolumeLive() { return in - out; }
    float getVolumeIn() { return in; }
    float getVolumeOut() { return out; }
};
