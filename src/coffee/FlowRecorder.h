/*
  FlowRecorder: the flow of the water During an extraction, kept as a curve for the journal (see settings/Journal.h).

  Every CURVE_SAMPLE_MS (Config.h) one sample is taken: the water that went through each sensor since the previous sample, in tenths of
  ml (0..255), In then Out. The tool that reads the journal turns them into flows (ml/s) and into the volume that came out. The
  samples are kept in RAM (2 x CURVE_SAMPLES bytes) until the journal writes them in the EEPROM. When the curve is full the end of
  the extraction is not recorded.

  Plain logic (no hardware): the machine gives it the volumes counted by the sensors, so that it is tested on the computer.
*/

#pragma once
#include <Arduino.h>
#include "../Config.h"

class FlowRecorder {
  public:
    // Starts a new curve (the previous one is dropped): the volumes of the sensors are 0 now.
    void begin(uint32_t now);

    // Call on every loop pass with the volumes counted since the start of the extraction (ml). Takes the samples that are due.
    void update(uint32_t now, float inMl, float outMl);

    // No more samples (the curve is kept until the next begin())
    void stop() { recording_ = false; }
    bool recording() const { return recording_; }

    // The samples: in[0], out[0], in[1], out[1]... (tenths of ml)
    uint16_t count() const { return count_; }
    const uint8_t *samples() const { return samples_; }

  private:
    uint8_t samples_[2 * CURVE_SAMPLES] = {};
    uint16_t count_ = 0;
    bool recording_ = false;
    uint32_t nextAt_ = 0;
    int32_t lastIn_ = 0;    // the volumes (tenths of ml) already in the samples
    int32_t lastOut_ = 0;
    static uint8_t takeDelta(float ml, int32_t &last);
};
