#!/bin/sh
# Host tests of the logic (no Arduino needed): ./test/run.sh
# The sources are copied to a build folder. Two flavours:
#   test_*.cpp      the flow sensor is replaced by test/mock/FlowSensor.h (values set by the test)
#   test_sim_*.cpp  the flow sensor is replaced by test/mock/FlowSimulator.h: the simulated water follows the pump relay
set -e
cd "$(dirname "$0")/.."
B=${TMPDIR:-/tmp}/coffee_screen_test
rm -rf "$B" && mkdir -p "$B/mock" "$B/real"
for d in mock real; do
  cp -r src "$B/$d/src"
  cp test/stubs/EEPROM.h "$B/$d/"
done
cp test/mock/FlowSensor.h "$B/mock/src/drivers/FlowSensor.h"
cp test/mock/FlowSimulator.h "$B/real/src/drivers/FlowSensor.h"
status=0
for t in test/test_*.cpp; do
  n=$(basename "$t" .cpp)
  case "$n" in test_sim_*) d=real;; *) d=mock;; esac
  EXTRA=""
  # sources linked with the test (the test provides the stubs of what it does not link)
  C="$B/$d/src/coffee/ExtractionController.cpp"
  case "$n" in
    test_flush|test_pre) C="$C $B/$d/src/settings/Settings.cpp $B/$d/src/display/ScreenController.cpp";;
    test_anticipation) C="$C $B/$d/src/settings/Settings.cpp";;
    test_journal) C="$B/$d/src/settings/Journal.cpp";;
    test_temperature) C="$B/$d/src/heating/TemperatureSensor.cpp";;
    test_encoder) C="$B/$d/src/drivers/CJMCU111.cpp";;
    test_heater) C="$B/$d/src/heating/HeaterController.cpp";;
    test_display|test_welcome|test_text) C="$B/$d/src/display/Display.cpp";;
    test_machine) C="$C $B/$d/src/settings/Journal.cpp $B/$d/src/settings/Settings.cpp $B/$d/src/display/ScreenController.cpp $B/$d/src/coffee/CoffeeMachine.cpp $B/$d/src/heating/HeaterController.cpp $B/$d/src/heating/TemperatureSensor.cpp $B/$d/src/drivers/CJMCU111.cpp";;
  esac
  g++ -std=c++11 -w $EXTRA -I"$B/$d" -Itest/stubs "$t" test/stubs/ArduinoStubs.cpp \
      $C -o "$B/$n" 2>"$B/$n.log" || { echo "$n: BUILD FAILED"; head -5 "$B/$n.log"; status=1; continue; }
  # the status of the test itself (not the one of "tail"): a failing test fails the script
  rc=0; out=$("$B/$n") || rc=$?   # (set -e: a failing test must not stop the script before the others)
  printf "%s: %s\n" "$n" "$(printf '%s\n' "$out" | tail -3)"
  [ $rc -eq 0 ] || status=1
done
exit $status
