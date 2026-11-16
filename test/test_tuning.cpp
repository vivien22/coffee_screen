// Tuning machine with the mock flow sensor: serial commands, pump and its safeties, telemetry.
#include <cstring>
#include <cstdio>
#include "src/Config.h"
#include "src/machine/TuningMachine.h"
#include <SPI.h>
#include "TestSupport.h"



// The MAX31865 answers the RTD register: the test sets the temperature it "measures" (PT100, Callendar-Van Dusen)
static uint16_t rtdRegister = 0; static int byteIndex = 0;
static uint8_t answer(uint8_t v) {
  if (v == 0x01) { byteIndex = 0; return 0; }
  if (v != 0) { byteIndex = -1; return 0; }
  if (byteIndex < 0) return 0;
  return byteIndex++ == 0 ? (uint8_t)(rtdRegister >> 8) : (uint8_t)rtdRegister;
}
static void setProbe(double celsius) {
  const double r = RNOMINAL * (1 + 3.9083e-3 * celsius - 5.775e-7 * celsius * celsius);
  rtdRegister = (uint16_t)((uint16_t)(r / RREF * 32768.0 + 0.5) << 1);
}

static TuningMachine *machine;
static void send(const char *text) {
  char line[40]; strncpy(line, text, sizeof line); line[sizeof line - 1] = 0;
  Serial.out.clear();
  machine->execute(line);
}
static bool said(const char *text) { return Serial.out.find(text) != std::string::npos; }

// Runs the machine for `ms` ms (a pass every 10 ms)
static void run(unsigned long ms) {
  const unsigned long end = t + ms;
  for (; t < end; t += 10) machine->update();
}

int main() {
  SPI.hook = answer;
  setProbe(90);
  TuningMachine m; machine = &m;
  m.begin();
  CHECK(pins[PIN_PUMP_RELAY] == 0 && pins[PIN_VALVE_RELAY] == 0 && pins[PIN_HEATER_RELAY] == 0);
  CHECK(Serial.out.find("ms,probe,setpoint") != std::string::npos);

  // ----- Commands -----
  send("kp 12.5");   CHECK(said("OK kp 12.500")); CHECK(m.heater().kp() == 12.5);
  send("KI 0.2");    CHECK(m.heater().ki() > 0.1999 && m.heater().ki() < 0.2001 && m.heater().kp() == 12.5);   // upper case accepted, other gains kept
  send("kd 300");    CHECK(m.heater().kd() == 300);
  send("kp -1");     CHECK(said("ERR") && m.heater().kp() == 12.5);
  send("ff 0.45");   CHECK(m.heater().feedforwardGain() > 0.449f && m.heater().feedforwardGain() < 0.451f);
  send("delay 2.5"); CHECK(m.heater().feedforwardDelayMs() == 2500);
  send("power 1500"); CHECK(m.heater().heaterPowerW() == 1500);
  send("inlet 18");  CHECK(m.heater().inletTemperature() == 18);
  send("heater 0");  CHECK(!m.heater().enabled());
  send("heater 1");  CHECK(m.heater().enabled());
  send("set 120");   CHECK(m.heater().setpoint() == PROBE_SETPOINT_MAX);
  send("set 98.5");  CHECK(m.heater().setpoint() > 98.49 && m.heater().setpoint() < 98.51);
  send("set abc");   CHECK(said("ERR") && m.heater().setpoint() > 98.49 && m.heater().setpoint() < 98.51);
  send("set");       CHECK(said("ERR"));
  send("dance 3");   CHECK(said("ERR"));
  send("help");      CHECK(said("# set"));
  send("status");    CHECK(said("# PID_KP 12.500") && said("# FEEDFORWARD_DELAY_MS 2500") && said("ms,probe"));
  send("stream 500"); CHECK(m.telemetryPeriodMs() == 500);

  // Commands arrive through the serial port too, one line at a time
  Serial.in = "kp 7\r\nki 0.5\n"; Serial.inPos = 0;
  run(20);
  CHECK(m.heater().kp() == 7 && m.heater().ki() == 0.5);

  // ----- Telemetry -----
  Serial.out.clear();
  send("stream 1000"); Serial.out.clear();
  run(2500);
  int lines = 0; for (char c : Serial.out) lines += (c == '\n');
  CHECK(lines == 2);
  const size_t at = Serial.out.find('\n');
  int commas = 0; for (size_t i = 0; i < at; i++) commas += (Serial.out[i] == ',');
  CHECK(commas == 10);
  CHECK(Serial.out.find(",90.0") != std::string::npos || Serial.out.find(",89.9") != std::string::npos);   // the probe
  send("stream 0"); Serial.out.clear(); run(3000); CHECK(Serial.out.empty());

  // ----- Pump: stops when the time is over -----
  FlowSensor *flow = nullptr; (void)flow;
  send("pump 5");     CHECK(said("OK pump on") && m.pumpIsOn());
  CHECK(pins[PIN_PUMP_RELAY] == 1 && pins[PIN_VALVE_RELAY] == 1);
  send("pump 0");     CHECK(!m.pumpIsOn() && pins[PIN_PUMP_RELAY] == 0 && pins[PIN_VALVE_RELAY] == 0);

  // No water counted (the mock sensor gives no volume): the no-flow safety stops the pump
  send("pump 30");    CHECK(m.pumpIsOn());
  Serial.out.clear();
  run(NO_FLOW_TIMEOUT_MS - 200);  CHECK(m.pumpIsOn());
  run(500);           CHECK(!m.pumpIsOn() && said("NO FLOW") && pins[PIN_PUMP_RELAY] == 0);

  // "stop": pump off and setpoint 0
  send("pump 30");    CHECK(m.pumpIsOn());
  send("stop");       CHECK(!m.pumpIsOn() && m.heater().setpoint() == 0 && said("OK stop"));

  // The duration is limited
  send("pump 1000");  CHECK(m.pumpIsOn());
  send("pump 0");

  // A sensor that is not valid is shown as nan, the heater stays off
  setProbe(150);
  send("set 98"); send("stream 1000"); Serial.out.clear();
  run(1500);
  CHECK(Serial.out.find(",nan,") != std::string::npos);
  CHECK(pins[PIN_HEATER_RELAY] == 0);

  printf(fails ? "%d FAIL\n" : "ALL OK\n", fails);
  return fails != 0;
}
