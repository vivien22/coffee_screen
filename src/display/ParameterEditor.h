/*
  ParameterEditor: the settings being edited by the user and the parameter selected, without any drawing.

  It holds the working values (not saved yet: the machine saves them), which parameter the encoder sets, the cycle of the double
  push, the limits of each value, and the cup weight entered in CAL with the correction of the loss it gives. ScreenController shows
  what it holds.

  Pure logic (no Arduino): tested through ScreenController and the machine (test/test_machine.cpp...).
*/

#pragma once
#include <stdint.h>
#include "../settings/Settings.h"

// Parameter selected on the screen (large digits), adjusted by the encoder.
// A double push selects the next one, in the order of this enum, then loops back to the volume.
// AUTO mode: volume, CAL (only after an extraction), pre-infusion, wait, flush, temperature. MANUAL mode: volume, MANUAL time,
// temperature (the other parameters are skipped).
// The pre-infusion volume is shown in the volume field with the "PRE" tag; the wait and flush times
// are shown in the duration field with the "WAIT" / "FLUSH" tag.
enum ActiveParameter {
  PARAMETER_VOLUME,
  PARAMETER_CALIBRATION,        // AUTO mode, after an extraction: the real weight of the cup (g), corrects the loss
  PARAMETER_PREINFUSION,        // pre-infusion volume (ml)
  PARAMETER_FLOW_LIMIT,         // AUTO, after the pre-infusion: flow limit of the extraction (ml/s, 0.1 steps), 0 = none
  PARAMETER_PREINFUSION_WAIT,   // wait time after the pre-infusion (s)
  PARAMETER_FLUSH,              // flush time after the extraction (s)
  PARAMETER_MANUAL_TIME,        // MANUAL mode: time of the extraction (s), 0 = until the second push
  PARAMETER_TEMPERATURE,
  PARAMETER_COUNT
};

class ParameterEditor {
  public:
    // The saved settings; the volume is selected
    void begin(const Settings &settings);

    const Settings &settings() const { return settings_; }   // working values (may not be saved)
    ActiveParameter active() const { return active_; }
    void select(ActiveParameter parameter) { active_ = parameter; }

    // The parameter after the selected one in the cycle of the double push, skipping those that do not exist in the mode:
    //   AUTO:   volume -> [CAL, only after an extraction] -> pre-infusion -> limit -> wait -> flush -> temperature -> volume
    //   MANUAL: volume -> MANUAL time -> temperature -> volume
    ActiveParameter next() const;

    // AUTO <-> MANUAL (the selected parameter is not changed: the caller selects the volume)
    void toggleMode() { settings_.manual = !settings_.manual; }

    // Adds delta to the selected parameter, kept between VALUE_MIN and its maximum (VOLUME_MAX, PREINFUSION_MAX_ML,
    // PREINFUSION_WAIT_MAX_SECONDS, FLUSH_MAX_SECONDS, MANUAL_MAX_SECONDS, TEMPERATURE_MAX...). steaming: the temperature is the
    // steaming one. Returns true if the value changed. In MANUAL mode the volume is not a target: it does not change.
    bool adjust(int16_t delta, bool steaming);

    // The temperature being set: the steaming one in the steaming mode
    int16_t temperatureSetting(bool steaming) const { return steaming ? settings_.steamingTemperature : settings_.temperature; }

    // What the fields show when nothing is going on: the target volume (0 in MANUAL: no target), and 0 s (the MANUAL time in MANUAL,
    // what will be counted down)
    uint16_t readyVolume() const { return settings_.manual ? 0 : settings_.volume; }
    uint16_t readyDuration() const { return settings_.manual ? settings_.manualSeconds : 0; }

    // Calibration of the loss (see Config.h). offerCalibration(): an AUTO extraction that gave estimateMl is over, the CAL
    // parameter joins the cycle, starting at estimateMl (the cup weight is entered there). withdrawCalibration(): it leaves the cycle
    // (new extraction). applyCalibration(netMl): CAL is validated; netMl is In - Out of the extraction. If a weight was entered the
    // loss moves toward the measured one by CALIBRATION_GAIN (kept between 0 and LOSS_MAX_ML) and true is returned (the caller
    // selects the volume again); otherwise nothing is corrected. CAL leaves the cycle either way.
    void offerCalibration(uint16_t estimateMl);
    void withdrawCalibration() { calibrationAvailable_ = false; }
    bool applyCalibration(float netMl);
    bool calibrationChanged() const { return calibrationValue_ != calibrationStart_; }   // a weight was entered in CAL
    int16_t calibrationValue() const { return calibrationValue_; }

  private:
    bool available(ActiveParameter parameter) const;   // the parameter exists in the current mode

    Settings settings_ = {};
    ActiveParameter active_ = PARAMETER_VOLUME;
    bool calibrationAvailable_ = false;
    int16_t calibrationValue_ = 0;   // the cup weight entered (starts at the estimate)
    int16_t calibrationStart_ = 0;   // the estimate of the extraction
};
