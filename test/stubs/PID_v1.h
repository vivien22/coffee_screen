// PID_v1 stand-in for the host: same algorithm as the Arduino library (proportional on error, integral term
// clamped to the output limits, derivative on the measurement), enough to see the controller work.
#pragma once
#include <Arduino.h>
#define AUTOMATIC 1
#define MANUAL 0
#define DIRECT 0
#define P_ON_E 1

class PID {
  public:
    PID(double *input, double *output, double *setpoint, double kp, double ki, double kd, int, int) :
      in_(input), out_(output), set_(setpoint), mode_(MANUAL), sampleMs_(100), lastAt_(0), lastIn_(0), iTerm_(0),
      min_(0), max_(255) { SetTunings(kp, ki, kd); }

    void SetTunings(double kp, double ki, double kd) {
      const double seconds = sampleMs_ / 1000.0;
      kp_ = kp; ki_ = ki * seconds; kd_ = kd / seconds;
    }
    void SetSampleTime(int ms) {
      const double ratio = (double)ms / sampleMs_;
      ki_ *= ratio; kd_ /= ratio; sampleMs_ = ms;
    }
    void SetMode(int mode) {
      if (mode == AUTOMATIC && mode_ == MANUAL) { iTerm_ = clamp(*out_); lastIn_ = *in_; lastAt_ = millis(); }
      mode_ = mode;
    }
    void SetOutputLimits(double min, double max) {
      min_ = min; max_ = max;
      if (mode_ == AUTOMATIC) { *out_ = clamp(*out_); iTerm_ = clamp(iTerm_); }
    }
    bool Compute() {
      if (mode_ != AUTOMATIC) return false;
      const unsigned long now = millis();
      if (now - lastAt_ < (unsigned long)sampleMs_) return false;
      const double error = *set_ - *in_;
      iTerm_ = clamp(iTerm_ + ki_ * error);
      *out_ = clamp(kp_ * error + iTerm_ - kd_ * (*in_ - lastIn_));
      lastIn_ = *in_; lastAt_ = now;
      return true;
    }
  private:
    double clamp(double v) const { return v > max_ ? max_ : (v < min_ ? min_ : v); }
    double *in_, *out_, *set_;
    int mode_, sampleMs_;
    unsigned long lastAt_;
    double lastIn_, iTerm_, min_, max_, kp_, ki_, kd_;
};
