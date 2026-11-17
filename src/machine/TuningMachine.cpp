// TuningMachine.cpp: the update pass, the pump with its safeties, the serial commands and the telemetry.

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "TuningMachine.h"
#include "../Config.h"

TuningMachine::TuningMachine() :
  temperature_(PIN_MAX31865_CS),
  heater_(PIN_HEATER_RELAY),
  pumpOn_(false),
  pumpEndAt_(0),
  noFlowCheckAt_(0),
  noFlowCheckMl_(0.0f),
  telemetryPeriodMs_(TELEMETRY_PERIOD_MS),
  lastTelemetryAt_(0),
  lineLength_(0)
{
  line_[0] = 0;
}

// Outputs first (heater, pump and valve off), then the sensors.
void TuningMachine::begin() {
  heater_.begin();

  pinMode(PIN_PUMP_RELAY, OUTPUT);
  pinMode(PIN_VALVE_RELAY, OUTPUT);
  digitalWrite(PIN_PUMP_RELAY, LOW);
  digitalWrite(PIN_VALVE_RELAY, LOW);

  temperature_.begin();
  flow_.begin();
  heater_.setSetpoint(DEFAULT_SETPOINT);

  Serial.println(F("# pid-tuning, type help"));
  printHeader();
  lastTelemetryAt_ = millis();
}

// One pass of the main loop, called as often as possible (no delay anywhere):
// commands, pump, temperature, heater, telemetry.
void TuningMachine::update() {
  readSerial();

  const uint32_t now = millis();

  flow_.update();
  updatePump(now);

  temperature_.update(now);
  heater_.update(now,
                 temperature_.isValid(),
                 temperature_.celsius(),
                 pumpOn_ ? flow_.getInflowRate() / 60.0f : 0.0f);

  if (telemetryPeriodMs_ > 0 && (uint32_t)(now - lastTelemetryAt_) >= telemetryPeriodMs_) {
    lastTelemetryAt_ = now;
    printTelemetry(now);
  }
}

// ------------------------------------------------------------
// Pump

// Pump and valve on together; stops after the given time.
void TuningMachine::startPump(uint32_t now, uint16_t seconds) {
  flow_.startExtraction();   // volumes back to 0

  digitalWrite(PIN_VALVE_RELAY, HIGH);
  digitalWrite(PIN_PUMP_RELAY, HIGH);

  pumpOn_ = true;
  pumpEndAt_ = now + (uint32_t)seconds * 1000UL;
  noFlowCheckAt_ = now;
  noFlowCheckMl_ = 0.0f;
}

void TuningMachine::stopPump(const __FlashStringHelper *reason) {
  digitalWrite(PIN_PUMP_RELAY, LOW);
  digitalWrite(PIN_VALVE_RELAY, LOW);
  flow_.stopExtraction();
  pumpOn_ = false;

  Serial.print(F("# pump off: "));
  Serial.println(reason);
}

// End of the requested time, and the no-flow safety: the water counted by the In sensor must increase by
// NO_FLOW_MIN_ML every NO_FLOW_TIMEOUT_MS.
void TuningMachine::updatePump(uint32_t now) {
  if (!pumpOn_) {
    return;
  }

  if ((int32_t)(now - pumpEndAt_) >= 0) {
    stopPump(F("time over"));
    return;
  }

  if ((uint32_t)(now - noFlowCheckAt_) >= NO_FLOW_TIMEOUT_MS) {
    const float volumeIn = flow_.getVolumeIn();
    if (volumeIn - noFlowCheckMl_ < NO_FLOW_MIN_ML) {
      stopPump(F("NO FLOW"));
      return;
    }
    noFlowCheckAt_ = now;
    noFlowCheckMl_ = volumeIn;
  }
}

// ------------------------------------------------------------
// Serial console

// Collects the characters of a line without ever waiting; a full line is executed.
void TuningMachine::readSerial() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();

    if (c == '\n' || c == '\r') {
      line_[lineLength_] = 0;
      lineLength_ = 0;
      execute(line_);
    } else if (lineLength_ < sizeof(line_) - 1) {
      line_[lineLength_++] = c;
    }
  }
}

// Reads a number that fills the whole text. false if there is none.
static bool parseNumber(const char *text, float &value) {
  char *end;
  value = (float)strtod(text, &end);
  return end != text && *end == 0;
}

// "name value": the name is in lower case, the value is optional ("pump" alone, "status"...).
void TuningMachine::execute(char *line) {
  char *argument = line;
  while (*argument && *argument != ' ') {
    *argument = (char)tolower(*argument);
    argument++;
  }
  if (*argument) {
    *argument++ = 0;
  }
  while (*argument == ' ') {
    argument++;
  }

  if (!*line) {
    return;
  }

  float value = 0.0f;
  const bool hasValue = parseNumber(argument, value);

  if (!strcmp(line, "help")) {
    printHelp();
  } else if (!strcmp(line, "status")) {
    printParameters();
    printHeader();
    printTelemetry(millis());
  } else if (!strcmp(line, "stop")) {
    // Everything off: the pump stops, the setpoint goes to 0 (the heater is off)
    if (pumpOn_) {
      stopPump(F("stop"));
    }
    digitalWrite(PIN_VALVE_RELAY, LOW);   // the valve too, even when it was opened by hand with "valve 1"
    heater_.setSetpoint(0);
    Serial.println(F("OK stop: pump and valve off, setpoint 0"));
  } else if (!hasValue) {
    Serial.println(F("ERR a number is expected (help)"));
  } else if (!strcmp(line, "set")) {
    heater_.setSetpoint(value);
    Serial.print(F("OK set ")); Serial.println(heater_.setpoint(), 1);
  } else if (!strcmp(line, "kp") || !strcmp(line, "ki") || !strcmp(line, "kd")) {
    if (value < 0.0f) {
      Serial.println(F("ERR gains are not negative"));
      return;
    }
    heater_.setTunings(line[1] == 'p' ? value : heater_.kp(),
                       line[1] == 'i' ? value : heater_.ki(),
                       line[1] == 'd' ? value : heater_.kd());
    Serial.print(F("OK ")); Serial.print(line); Serial.print(' '); Serial.println(value, 3);
  } else if (!strcmp(line, "ff")) {
    heater_.setFeedforwardGain(value < 0.0f ? 0.0f : value);
    Serial.print(F("OK ff ")); Serial.println(heater_.feedforwardGain(), 3);
  } else if (!strcmp(line, "delay")) {
    // seconds
    heater_.setFeedforwardDelayMs(value < 0.0f ? 0UL : (uint32_t)(value * 1000.0f));
    Serial.print(F("OK delay ")); Serial.println(heater_.feedforwardDelayMs() / 1000.0f, 2);
  } else if (!strcmp(line, "power")) {
    heater_.setHeaterPowerW(value);
    Serial.print(F("OK power ")); Serial.println(heater_.heaterPowerW(), 0);
  } else if (!strcmp(line, "inlet")) {
    heater_.setInletTemperature(value);
    Serial.print(F("OK inlet ")); Serial.println(heater_.inletTemperature(), 1);
  } else if (!strcmp(line, "heater")) {
    heater_.setEnabled(value != 0.0f);
    Serial.print(F("OK heater ")); Serial.println(heater_.enabled() ? 1 : 0);
  } else if (!strcmp(line, "stream")) {
    // milliseconds, 0 = off
    telemetryPeriodMs_ = value < 0.0f ? 0UL : (uint32_t)value;
    Serial.print(F("OK stream ")); Serial.println(telemetryPeriodMs_);
  } else if (!strcmp(line, "valve")) {
    // the valve alone, open or closed until the next "valve" or "stop" (the pump command opens it too)
    digitalWrite(PIN_VALVE_RELAY, value != 0.0f ? HIGH : LOW);
    Serial.print(F("OK valve ")); Serial.println(value != 0.0f ? 1 : 0);
  } else if (!strcmp(line, "pump")) {
    // seconds, 0 = stop
    if (value <= 0.0f) {
      if (pumpOn_) {
        stopPump(F("stop"));
      }
    } else {
      startPump(millis(), value > PUMP_MAX_SECONDS ? PUMP_MAX_SECONDS : (uint16_t)value);
      Serial.println(F("OK pump on"));
    }
  } else {
    Serial.println(F("ERR unknown command (help)"));
  }
}

// ------------------------------------------------------------
// Output

// The values to copy into Config.h once the tuning is good
void TuningMachine::printParameters() {
  Serial.print(F("# set ")); Serial.print(heater_.setpoint(), 1);
  Serial.print(F("  heater ")); Serial.println(heater_.enabled() ? 1 : 0);
  Serial.print(F("# PID_KP ")); Serial.println(heater_.kp(), 3);
  Serial.print(F("# PID_KI ")); Serial.println(heater_.ki(), 3);
  Serial.print(F("# PID_KD ")); Serial.println(heater_.kd(), 3);
  Serial.print(F("# FEEDFORWARD_GAIN ")); Serial.println(heater_.feedforwardGain(), 3);
  Serial.print(F("# FEEDFORWARD_DELAY_MS ")); Serial.println(heater_.feedforwardDelayMs());
  Serial.print(F("# HEATER_POWER_W ")); Serial.println(heater_.heaterPowerW(), 0);
  Serial.print(F("# WATER_INLET_TEMPERATURE ")); Serial.println(heater_.inletTemperature(), 1);
}

void TuningMachine::printHeader() {
  Serial.println(F("ms,probe,setpoint,pid_ms,ff_ms,on_ms,heater,pump,flow_in,vol_in,vol_out"));
}

// ms: time since start. probe: degrees C ("nan" when the sensor is not valid). pid_ms, ff_ms, on_ms: heating
// time of the current window (PID part, feedforward part, total). heater: relay state. pump: 0/1.
// flow_in: ml/s. vol_in, vol_out: ml since the pump started.
void TuningMachine::printTelemetry(uint32_t now) {
  Serial.print(now); Serial.print(',');
  if (temperature_.isValid()) {
    Serial.print(temperature_.celsius(), 2);
  } else {
    Serial.print(F("nan"));
  }
  Serial.print(','); Serial.print(heater_.setpoint(), 1);
  Serial.print(','); Serial.print(heater_.pidMs());
  Serial.print(','); Serial.print(heater_.feedforwardMs());
  Serial.print(','); Serial.print(heater_.onTimeMs());
  Serial.print(','); Serial.print(heater_.isOn() ? 1 : 0);
  Serial.print(','); Serial.print(pumpOn_ ? 1 : 0);
  Serial.print(','); Serial.print(pumpOn_ ? flow_.getInflowRate() / 60.0f : 0.0f, 2);
  Serial.print(','); Serial.print(flow_.getVolumeIn(), 1);
  Serial.print(','); Serial.println(flow_.getVolumeOut(), 1);
}

void TuningMachine::printHelp() {
  Serial.println(F("# set <C>      probe setpoint (0 = heater off), max " STRINGIFY(PROBE_SETPOINT_MAX)));
  Serial.println(F("# kp|ki|kd <v> PID gains"));
  Serial.println(F("# ff <gain>    feedforward gain (0 = off, 1 = full estimate)"));
  Serial.println(F("# delay <s>    feedforward delay, seconds"));
  Serial.println(F("# power <W>    heater power;  inlet <C> inlet water temperature"));
  Serial.println(F("# heater <0|1> heater disabled / enabled"));
  Serial.println(F("# valve <0|1>  the valve alone: open / closed"));
  Serial.println(F("# pump <s>     pump + valve for s seconds (0 = stop)"));
  Serial.println(F("# stream <ms>  telemetry period (0 = off)"));
  Serial.println(F("# status       parameters (to copy into Config.h) and one line"));
  Serial.println(F("# stop         pump and valve off, setpoint 0"));
}
