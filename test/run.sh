#!/bin/sh
# Host tests of the logic (no Arduino needed): ./test/run.sh
# The sources are copied to a build folder. Two flavours:
#   test_*.cpp      the flow sensor is replaced by test/mock/FlowSensor.h (values set by the test)
#   test_sim_*.cpp  the real FlowSimulator (FLOW_SIMULATOR in Config.h) follows the pump relay
set -e
cd "$(dirname "$0")/.."
B=${TMPDIR:-/tmp}/coffee_screen_test
rm -rf "$B" && mkdir -p "$B/mock" "$B/real"
for d in mock real; do
  cp -r src "$B/$d/src"
done
cp test/mock/FlowSensor.h "$B/mock/src/drivers/FlowSensor.h"
# FLOW_SIMULATOR is commented out in Config.h (the real flow sensor): the copy of the test_sim_* flavour switches it on
sed -i 's|^// #define FLOW_SIMULATOR|#define FLOW_SIMULATOR|' "$B/real/src/Config.h"
status=0
for t in test/test_*.cpp; do
  n=$(basename "$t" .cpp)
  case "$n" in test_sim_*) d=real;; *) d=mock;; esac
  # sources linked with the test (the test provides the stubs of what it does not link)
  C=""
  case "$n" in
    test_heater) C="$B/$d/src/heating/HeaterController.cpp";;
    test_temperature) C="$B/$d/src/heating/TemperatureSensor.cpp";;
    test_tuning|test_sim_tuning) C="$B/$d/src/machine/TuningMachine.cpp $B/$d/src/heating/HeaterController.cpp $B/$d/src/heating/TemperatureSensor.cpp";;
  esac
  g++ -std=c++11 -w -I"$B/$d" -Itest/stubs "$t" test/stubs/ArduinoStubs.cpp \
      $C -o "$B/$n" 2>"$B/$n.log" || { echo "$n: BUILD FAILED"; head -5 "$B/$n.log"; status=1; continue; }
  # the exit status of the test itself counts (not the one of tail)
  printf "%s: " "$n"; "$B/$n" > "$B/$n.out" || status=1; tail -3 "$B/$n.out"
done
exit $status
