/*
  screen_controller: what the screen shows and when it is redrawn.

  It holds the working values edited by the user (not saved yet), the selected parameter
  and the value currently shown in each field. Fields are redrawn only when marked dirty.
  The only module that calls display.h; it knows nothing about the sensors or the heater,
  which are pushed to it through the show...() methods.
*/

#pragma once
#include <Arduino.h>
#include "settings.h"
#include "display.h"   // PhaseTag

// Parameter selected on the screen (large digits), adjusted by the encoder.
// A double push selects the next one, in the order of this enum, then loops back to the volume.
// The pre-infusion, wait and flush times are shown in the duration field, with the "PRE" / "WAIT" /
// "FLUSH" tag.
enum ActiveParameter {
  PARAMETER_VOLUME,
  PARAMETER_PREINFUSION,        // pre-infusion time (s)
  PARAMETER_PREINFUSION_WAIT,   // wait time after the pre-infusion (s)
  PARAMETER_FLUSH,              // flush time after the extraction (s)
  PARAMETER_TEMPERATURE,
  PARAMETER_COUNT
};

class ScreenController {
  public:
    // Initialize the display; the first drawing is done by the first update().
    void begin(const Settings &settings);

    // Call on every loop pass: end of the "saved" blink, redraw of the dirty fields.
    void update(uint32_t now);

    // ----- User actions -----
    ActiveParameter activeParameter() const { return active_; }
    const Settings &settings() const { return settings_; }   // working values (may not be saved)

    void selectNextParameter();

    // Add delta to the selected parameter, kept between VALUE_MIN and its maximum
    // (VOLUME_MAX, PREINFUSION_MAX_SECONDS, FLUSH_MAX_SECONDS, TEMPERATURE_MAX).
    // Returns true if the value changed.
    bool adjustActiveParameter(int16_t delta);

    // The selected value disappears for SAVE_BLINK_MS: feedback of a save
    void confirmSave(uint32_t now);

    // ----- Machine state to show -----
    void showLiveTemperature(uint32_t now, double celsius);
    void showHeaterState(bool on);
    void showExtractionStarted();
    void showExtractionProgress(uint16_t volumeMl, uint16_t durationSeconds);

    // Pre-infusion: the duration counts down the seconds left in the phase and a tag is shown
    // after it ("PRE" while the pump runs, "WAIT" while the puck soaks).
    void showPreinfusion(uint16_t volumeMl, uint16_t secondsLeft, bool waiting);

    // Flush: the "FLUSH" tag is shown after the duration. While waiting for the push the duration is
    // the one of the extraction; while flushing it counts down the seconds left in the flush and
    // the tag blinks (flushing = true). The volume shown is the one of the extraction.
    void showFlush(uint16_t volumeMl, uint16_t secondsLeft, bool flushing);

  private:
    enum DirtyFlags : uint8_t {
      DIRTY_NONE        = 0,
      DIRTY_VOLUME      = 1 << 0,
      DIRTY_TEMPERATURE = 1 << 1,
      DIRTY_DURATION    = 1 << 2,   // the whole field: number, unit and tag
      DIRTY_HEATER      = 1 << 3,
      DIRTY_DURATION_NUMBER = 1 << 4,   // the number only: the tag already on the screen is kept
      DIRTY_TAG         = 1 << 5,   // the tag only: it blinks
      DIRTY_ALL_VALUES  = DIRTY_VOLUME | DIRTY_TEMPERATURE | DIRTY_DURATION
    };

    void render();
    void showExtractionValues(uint16_t volumeMl, uint16_t durationValue, PhaseTag tag);

    Settings settings_;
    ActiveParameter active_ = PARAMETER_VOLUME;

    // What each field shows
    int16_t volumeShown_ = 0;
    int16_t durationShown_ = 0;
    PhaseTag tagShown_ = PHASE_TAG_NONE;   // tag drawn after the duration

    // Blinking of the "FLUSH" tag (during FLUSHING)
    bool tagBlinking_ = false;
    bool tagBlinkPending_ = false;   // blinking just started: the first toggle time is set by update()
    bool tagVisible_ = true;
    uint32_t tagToggleAt_ = 0;
    double temperatureShown_ = 0.0;

    double liveTemperature_ = 0.0;
    int16_t temperatureKey_ = -1;       // last displayed live temperature (tenths), -1 = none
    uint32_t lastTemperatureAt_ = 0;

    bool heaterShown_ = false;
    bool heaterKnown_ = false;

    uint8_t dirty_ = DIRTY_NONE;

    bool blinking_ = false;
    uint8_t blinkField_ = DIRTY_NONE;
    uint32_t blinkEndsAt_ = 0;
};
