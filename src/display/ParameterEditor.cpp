// ParameterEditor.cpp: see ParameterEditor.h

#include "ParameterEditor.h"
#include "../Config.h"

void ParameterEditor::begin(const Settings &settings) {
  settings_ = settings;
  active_ = PARAMETER_VOLUME;
}

// The parameters that exist in the current mode: the pre-infusion, the limit, the wait and the flush are AUTO only; the MANUAL time
// is MANUAL only; CAL only after an extraction.
bool ParameterEditor::available(ActiveParameter parameter) const {
  if (settings_.manual) {
    return parameter == PARAMETER_VOLUME || parameter == PARAMETER_MANUAL_TIME || parameter == PARAMETER_TEMPERATURE;
  }
  return parameter != PARAMETER_MANUAL_TIME && (parameter != PARAMETER_CALIBRATION || calibrationAvailable_);
}

ActiveParameter ParameterEditor::next() const {
  ActiveParameter next = active_;
  do {
    next = (ActiveParameter)((next + 1) % PARAMETER_COUNT);
  } while (!available(next));
  return next;
}

bool ParameterEditor::adjust(int16_t delta, bool steaming) {
  if (settings_.manual && active_ == PARAMETER_VOLUME) {
    return false;   // MANUAL mode: the volume is not a target, the field shows 0
  }

  int16_t *value;
  int16_t maxValue;
  switch (active_) {
    case PARAMETER_VOLUME:           value = &settings_.volume;          maxValue = VOLUME_MAX;                    break;
    case PARAMETER_PREINFUSION:      value = &settings_.preinfusion;     maxValue = PREINFUSION_MAX_ML;            break;
    case PARAMETER_FLOW_LIMIT:       value = &settings_.flowLimit;       maxValue = FLOW_LIMIT_MAX_TENTHS;         break;
    case PARAMETER_PREINFUSION_WAIT: value = &settings_.preinfusionWait; maxValue = PREINFUSION_WAIT_MAX_SECONDS;  break;
    case PARAMETER_FLUSH:            value = &settings_.flush;           maxValue = FLUSH_MAX_SECONDS;             break;
    case PARAMETER_MANUAL_TIME:      value = &settings_.manualSeconds;   maxValue = MANUAL_MAX_SECONDS;            break;
    case PARAMETER_CALIBRATION:      value = &calibrationValue_;         maxValue = VOLUME_MAX;                    break;
    default:
      value = steaming ? &settings_.steamingTemperature : &settings_.temperature;
      maxValue = TEMPERATURE_MAX;
      break;
  }

  int16_t newValue = constrain(*value + delta, VALUE_MIN, maxValue);
  // The flow limit is 0 (off) or from FLOW_LIMIT_MIN_TENTHS to the maximum: no value in between
  if (active_ == PARAMETER_FLOW_LIMIT && newValue > 0 && newValue < FLOW_LIMIT_MIN_TENTHS) {
    newValue = (delta > 0) ? FLOW_LIMIT_MIN_TENTHS : 0;
  }
  if (newValue == *value) {
    return false;
  }
  *value = newValue;
  return true;
}

void ParameterEditor::offerCalibration(uint16_t estimateMl) {
  calibrationAvailable_ = true;
  calibrationValue_ = calibrationStart_ = estimateMl;
}

bool ParameterEditor::applyCalibration(float netMl) {
  const bool changed = calibrationChanged();
  if (changed) {
    const float loss = settings_.loss + CALIBRATION_GAIN * (netMl - calibrationValue_ - settings_.loss);
    settings_.loss = constrain(loss, 0.0f, LOSS_MAX_ML);
  }
  calibrationAvailable_ = false;
  return changed;
}
