// Tuning machine with the real FlowSimulator: the water follows the pump relay and reaches the feedforward.
#include <cstring>
#include <cstdio>
#include "src/Config.h"
#include "src/coffee/TuningMachine.h"
#include <SPI.h>
#include "TestSupport.h"



static uint16_t rtdRegister = 0; static int byteIndex = 0;
static uint8_t answer(uint8_t v) {
  if (v == 0x01) { byteIndex = 0; return 0; }
  if (v != 0) { byteIndex = -1; return 0; }
  if (byteIndex < 0) return 0;
  return byteIndex++ == 0 ? (uint8_t)(rtdRegister >> 8) : (uint8_t)rtdRegister;
}

int main() {
  SPI.hook = answer;
  const double r = RNOMINAL * (1 + 3.9083e-3 * 98 - 5.775e-7 * 98 * 98);   // the probe reads 98 degrees
  rtdRegister = (uint16_t)((uint16_t)(r / RREF * 32768.0 + 0.5) << 1);

  TuningMachine m; m.begin();
  char line[24];
  strcpy(line, "set 98");  m.execute(line);
  strcpy(line, "kp 0");    m.execute(line);
  strcpy(line, "ki 0");    m.execute(line);
  strcpy(line, "kd 0");    m.execute(line);
  strcpy(line, "stream 0"); m.execute(line);
  strcpy(line, "delay 2"); m.execute(line);
  strcpy(line, "pump 10"); m.execute(line);

  int firstFeedforward = -1, lastFeedforward = -1;
  for (t = 0; t < 20000; t += 10) {
    m.update();
    if (m.heater().feedforwardMs() > 0) {
      if (firstFeedforward < 0) firstFeedforward = (int)t;
      lastFeedforward = (int)t;
    }
  }
  printf("pump off: %d, feedforward from %d ms to %d ms\n", !m.pumpIsOn(), firstFeedforward, lastFeedforward);
  CHECK(!m.pumpIsOn() && pins[PIN_PUMP_RELAY] == 0);
  // The pump runs from 0 to 10 s; with the 2 s delay the feedforward runs about from 2 s to 12 s
  CHECK(firstFeedforward >= 1800 && firstFeedforward <= 2800);
  CHECK(lastFeedforward >= 11800 && lastFeedforward <= 12800);

  // The volume counted is 3 ml/s for 10 s
  Serial.out.clear();
  strcpy(line, "status"); m.execute(line);
  const size_t last = Serial.out.rfind('\n', Serial.out.size() - 2);
  printf("%s", Serial.out.substr(last + 1).c_str());
  CHECK(Serial.out.find(",30.") != std::string::npos);

  printf(fails ? "%d FAIL\n" : "ALL OK\n", fails);
  return fails != 0;
}
