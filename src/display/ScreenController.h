/*
  ScreenController: what the screen shows and when it is redrawn.

  It holds the working values edited by the user (not saved yet), the selected parameter
  and the value currently shown in each field. Fields are redrawn only when marked dirty.
  The only module that calls Display.h; it knows nothing about the sensors or the heater,
  which are pushed to it through the show...() methods.
*/

#pragma once
#include <Arduino.h>
#include "../settings/Settings.h"
#include "Display.h"   // PhaseTag

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

class ScreenController {
  public:
    // Initialize the display and show the welcome picture (the backlight goes on). The real screen is not drawn yet:
    // do what has to be done at start-up (it takes place while the picture is shown), then finishWelcome(),
    // update() and show().
    void begin(const Settings &settings);

    // Waits until the welcome picture has been shown WELCOME_MS since begin(), clears it in the dark, and draws the
    // icons. Blocks for the rest of that time: call it once, at start-up.
    void finishWelcome();

    // Switch the backlight on. Call it once after the first update(): the icons and the values then appear together.
    void show();

    // The standby: the welcome picture replaces the screen, and nothing else is drawn (update() does nothing) until
    // leaveStandby(), which draws the real screen again in full, in the dark, and lights it. Both do nothing
    // if there is no welcome picture (WELCOME_MS 0): the screen is then left as it is.
    void enterStandby();
    void leaveStandby();

    // Call on every loop pass: blinking of the FLUSH tag and of the drop, redraw of the dirty fields.
    void update(uint32_t now);

    // ----- User actions -----
    ActiveParameter activeParameter() const { return active_; }
    const Settings &settings() const { return settings_; }   // working values (may not be saved)

    void selectNextParameter();

    // The volume (the main parameter) is selected again, from whichever parameter was being set (the values stay as they are).
    void selectVolume() { if (active_ != PARAMETER_VOLUME) activate(PARAMETER_VOLUME); }

    // Steaming mode: the screen shows the temperature alone, in large digits (the rest of the screen is cleared); the encoder sets
    // the steaming temperature (shown STEAM_SETPOINT_SHOW_MS after each turn, then the measured one again). leaveSteaming()
    // draws the real screen again in full, with the volume selected.
    void enterSteaming();
    void leaveSteaming();
    bool steaming() const { return steaming_; }

    // Switch between AUTO and MANUAL: the letter in the drop changes (the machine saves the mode).
    // The volume is selected again.
    void toggleMode();

    // Calibration of the loss (see Config.h). offerCalibration(): an AUTO extraction that gave
    // estimateMl is over, the CAL parameter joins the cycle of the double push, starting at estimateMl
    // (the cup weight is entered there). withdrawCalibration(): it leaves the cycle (new extraction).
    // applyCalibration(netMl): the double push that validates CAL; netMl is In - Out of the extraction;
    // corrects settings().loss and selects the volume again; if the value was not changed, nothing is
    // corrected and CAL stays selected (the caller goes on to the next parameter). Returns the loss.
    // CAL leaves the cycle.
    void offerCalibration(uint16_t estimateMl);
    void withdrawCalibration();
    float applyCalibration(float netMl);
    bool calibrationChanged() const { return calibrationValue_ != calibrationStart_; }   // a weight was entered in CAL
    uint8_t calibrationWeight() const { return (uint8_t)calibrationValue_; }

    // Add delta to the selected parameter, kept between VALUE_MIN and its maximum
    // (VOLUME_MAX, PREINFUSION_MAX_ML, PREINFUSION_WAIT_MAX_SECONDS, FLUSH_MAX_SECONDS, MANUAL_MAX_SECONDS, TEMPERATURE_MAX).
    // Returns true if the value changed.
    bool adjustActiveParameter(int16_t delta);


    // ----- Machine state to show -----
    void showLiveTemperature(uint32_t now, double celsius);
    // Temperature sensor fault: "ERR" replaces the live temperature until showLiveTemperature() is
    // called again (the setpoint is still shown while it is edited). Call it on every pass.
    void showTemperatureError();
    void showHeaterState(bool on);
    void showExtractionStarted();
    void showExtractionProgress(uint16_t volumeMl, uint16_t durationSeconds);

    // Pre-infusion: the volume field counts down the millilitres left to send, with the "PRE" tag;
    // the duration of the extraction runs as usual.
    void showPreinfusion(uint16_t mlLeft, uint16_t durationSeconds);

    // Pre-infusion wait: the duration field counts down the seconds left, with the "WAIT" tag.
    void showPreinfusionWait(uint16_t volumeMl, uint16_t secondsLeft);

    // Ready for the next extraction: the target volume and a duration of 0 s are shown, no tag.
    // In MANUAL mode the volume is 0 (no target) and the duration is the MANUAL time (what will be
    // counted down), 0 if it is 0.
    void showReady();

    // Flush: the "FLUSH" tag is shown after the duration. While waiting for the push the duration is
    // the one of the extraction; while flushing it counts down the seconds left in the flush and
    // the tag blinks (flushing = true). The volume shown is the one of the extraction.
    void showFlush(uint16_t volumeMl, uint16_t secondsLeft, bool flushing);

    // No-water error: the drop icon blinks (DROP_BLINK_MS) while true, and is shown again when false.
    // Call it on every pass: it only acts when the value changes.
    void showNoWater(bool error);

  private:
    enum DirtyFlags : uint8_t {
      DIRTY_NONE        = 0,
      DIRTY_VOLUME      = 1 << 0,   // the whole field: number, unit and tag
      DIRTY_TEMPERATURE = 1 << 1,
      DIRTY_DURATION    = 1 << 2,   // the whole field: number, unit and tag
      DIRTY_HEATER      = 1 << 3,
      DIRTY_DURATION_NUMBER = 1 << 4,   // the number only: the tag already on the screen is kept
      DIRTY_TAG         = 1 << 5,   // the tag of the duration only: it blinks
      DIRTY_VOLUME_NUMBER = 1 << 6,   // the number of the volume only: the tag already on the screen is kept
      DIRTY_ALL_VALUES  = DIRTY_VOLUME | DIRTY_TEMPERATURE | DIRTY_DURATION
    };

    void render(uint32_t now);
    void redrawScreen();                        // the real screen (or the steaming one) in full
    void activate(ActiveParameter next, bool animate = false);   // select a parameter and show it (animate: the rows fade, see FADE_OUT_MS)
    bool availableInMode(ActiveParameter p) const;
    bool calibrationAvailable_ = false;
    int16_t calibrationValue_ = 0;   // the cup weight entered (starts at the estimate)
    int16_t calibrationStart_ = 0;   // the estimate of the extraction
    uint16_t readyVolume() const;
    uint16_t readyDuration() const;
    void showExtractionValues(uint16_t volumeValue, PhaseTag volumeTag, uint16_t durationValue, PhaseTag durationTag);

    Settings settings_;
    ActiveParameter active_ = PARAMETER_VOLUME;

    // What each field shows
    int16_t volumeShown_ = 0;
    int16_t durationShown_ = 0;
    PhaseTag volumeTagShown_ = PHASE_TAG_NONE;     // tag drawn in the volume field
    PhaseTag durationTagShown_ = PHASE_TAG_NONE;   // tag drawn in the duration field

    // Blinking of the "FLUSH" tag (during FLUSHING)
    bool tagBlinking_ = false;
    bool tagBlinkPending_ = false;   // blinking just started: the first toggle time is set by update()
    bool tagVisible_ = true;
    uint32_t tagToggleAt_ = 0;
    double temperatureShown_ = 0.0;

    // Blinking of the drop icon (no-water error)
    bool dropBlinking_ = false;
    bool dropBlinkPending_ = false;
    bool dropVisible_ = true;
    uint32_t dropToggleAt_ = 0;

    bool steaming_ = false;               // steaming mode: only the temperature is shown
    bool steamSetpointShown_ = false;     // steaming: the setpoint is shown (just set), not the measured temperature
    uint32_t steamSetpointUntil_ = 0;
    double liveTemperature_ = 0.0;
    int16_t temperatureKey_ = -1;       // last displayed live temperature (tenths), -1 = none
    bool temperatureError_ = false;     // sensor fault: "ERR" is shown instead of the live temperature
    bool temperatureErrorShown_ = false; // "ERR" is on the screen now (it is not drawn again while it is)
    uint32_t lastTemperatureAt_ = 0;

    bool heaterShown_ = false;
    bool heaterKnown_ = false;

    uint8_t dirty_ = DIRTY_NONE;

    // Rolling digits (see ROLL_VOLUME_MS in Config.h): the number of the volume or of the duration that changes during an extraction
    // rolls from the old one to the new one. The frames are drawn by update(), at most every ROLL_FRAME_MS.
    struct Roll {
      bool requested = false;     // a new number came: render() starts the roll
      bool active = false;        // the frames are being drawn
      bool finishFirst = false;   // a new number came during the roll: the number of that roll is drawn as it is, then the new roll starts
      int16_t finishValue = 0;    // that number
      uint32_t start = 0;
      uint32_t nextFrame = 0;
    };
    Roll volumeRoll_, durationRoll_;
    uint32_t lastNow_ = 0;      // the time of the last update(): the rolls started by a redraw outside of update() use it
    void drawVolumeValue(int16_t value, bool redrawTag, uint8_t progress);
    void drawDurationValue(int16_t value, bool redrawTag, uint8_t progress);
    void stepRolls(uint32_t now);

    // Change of parameter (see FADE_OUT_MS in Config.h): the rows whose look changes fade out, then the new ones fade in.
    enum Row : uint8_t { ROW_TEMPERATURE, ROW_VOLUME, ROW_DURATION, ROW_COUNT };
    enum LookKind : uint8_t { LOOK_VALUE, LOOK_ERROR, LOOK_PREINFUSION, LOOK_FLOW_LIMIT, LOOK_CALIBRATION, LOOK_WAIT, LOOK_FLUSH, LOOK_MANUAL };
    // What a row shows: kind of the content, size of the digits, tag and number. Two rows with the same kind, size and tag differ only by the number.
    struct RowLook {
      uint8_t kind = LOOK_VALUE;
      uint8_t size = TEXT_SIZE_SMALL;
      uint8_t tag = PHASE_TAG_NONE;
      double value = 0.0;
      bool sameShape(const RowLook &other) const { return kind == other.kind && size == other.size && tag == other.tag; }
      bool operator==(const RowLook &other) const { return sameShape(other) && value == other.value; }
    };
    struct Fade {
      bool active = false;
      bool started = false;     // the start time is taken by the first update() after the change
      bool dimming = true;      // out phase (the old look) or in phase (the new one)
      uint8_t level = 255;      // brightness of the last frame drawn
      RowLook from, to;
      uint32_t start = 0;
      uint32_t nextFrame = 0;
    };
    Fade fade_[ROW_COUNT];
    RowLook lookOf(Row row, ActiveParameter active) const;
    void drawLook(Row row, const RowLook &look, uint8_t brightness, bool erase = true);
    uint8_t dirtyOf(Row row) const;
    uint8_t fadingRows() const;
    void finishFades();      // the rows in a fade show their look as it is, at once
    void stepFades(uint32_t now);

    // When the welcome picture was shown (millis). The last member, and only with a picture: a member in the middle
    // of the class moves the ones after it, and the far ones of an object cost more code on the AVR.
#if WELCOME_MS > 0
    uint32_t welcomeStartedAt_ = 0;
    bool standbyShown_ = false;         // the picture of the standby is on the screen: nothing else is drawn
#endif

};
