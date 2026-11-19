#!/bin/sh
# Host tests of the logic (no Arduino needed): ./test/run.sh
# The sources are copied to a build folder. Four flavours (the default Config.h drives the pump and the valve):
#   test_*.cpp      the flow sensor is replaced by test/mock/FlowSensor.h (values set by the test)
#   test_sim_*.cpp  the real FlowSimulator (FLOW_SIMULATOR, switched on in the copy of Config.h) follows the pump relay
#   test_boiler_only.cpp  DRIVE_PUMP and DRIVE_VALVE switched off in the copy: the heater only, the pump and the valve never go on
#   test_valve_by_hand.cpp  DRIVE_VALVE switched off in the copy: the pump works, the valve output never goes on
set -e
cd "$(dirname "$0")/.."
B=${TMPDIR:-/tmp}/coffee_screen_test
rm -rf "$B" && mkdir -p "$B/mock" "$B/real" "$B/boiler" "$B/pump"
for d in mock real boiler pump; do
  cp -r src "$B/$d/src"
  cp test/stubs/EEPROM.h "$B/$d/"
done
cp test/mock/FlowSensor.h "$B/mock/src/drivers/FlowSensor.h"
cp test/mock/FlowSensor.h "$B/boiler/src/drivers/FlowSensor.h"
cp test/mock/FlowSensor.h "$B/pump/src/drivers/FlowSensor.h"
# DRIVE_VALVE is switched off in the copy for the test of a valve operated by hand
sed -i 's|^#define DRIVE_VALVE$|// #define DRIVE_VALVE|' "$B/pump/src/Config.h"
# the copy for the boiler-only test switches the pump and the valve off
sed -i 's|^#define DRIVE_PUMP$|// #define DRIVE_PUMP|; s|^#define DRIVE_VALVE$|// #define DRIVE_VALVE|' "$B/boiler/src/Config.h"
# FLOW_SIMULATOR is commented out in Config.h (the real flow sensors): the copy of the test_sim_* flavour switches it on
sed -i 's|^// #define FLOW_SIMULATOR|#define FLOW_SIMULATOR|' "$B/real/src/Config.h"
status=0
for t in test/test_*.cpp; do
  n=$(basename "$t" .cpp)
  case "$n" in test_hires_*) continue;; esac   # built with DISPLAY_HIRES, below
  case "$n" in test_sim_*) d=real;; test_boiler_only) d=boiler;; test_valve_by_hand) d=pump;; *) d=mock;; esac
  # sources linked with the test (the test provides the stubs of what it does not link)
  C="$B/$d/src/machine/ExtractionController.cpp"
  case "$n" in
    test_flush|test_pre) C="$C $B/$d/src/settings/Settings.cpp $B/$d/src/display/ScreenController.cpp";;
    test_temperature) C="$B/$d/src/heating/TemperatureSensor.cpp";;
    test_encoder) C="$B/$d/src/drivers/CJMCU111.cpp";;
    test_heater) C="$B/$d/src/heating/HeaterController.cpp";;
    test_display|test_welcome) C="$B/$d/src/display/Display.cpp";;
    test_machine|test_boiler_only|test_valve_by_hand) C="$C $B/$d/src/settings/Settings.cpp $B/$d/src/display/ScreenController.cpp $B/$d/src/machine/CoffeeMachine.cpp $B/$d/src/heating/HeaterController.cpp $B/$d/src/heating/TemperatureSensor.cpp $B/$d/src/drivers/CJMCU111.cpp";;
  esac
  g++ -std=c++11 -w -I"$B/$d" -Itest/stubs "$t" test/stubs/ArduinoStubs.cpp \
      $C -o "$B/$n" 2>"$B/$n.log" || { echo "$n: BUILD FAILED"; head -5 "$B/$n.log"; status=1; continue; }
  out=$("$B/$n"); rc=$?
  printf "%s: %s\n" "$n" "$(printf '%s\n' "$out" | tail -3)"
  [ $rc -eq 0 ] || status=1
done
# The high resolution look of the Nano R4 (DISPLAY_HIRES): the property test of the display again, and the test of the pictures
for n in test_display test_hires_welcome; do
  g++ -std=c++11 -w -DDISPLAY_HIRES -I"$B/mock" -Itest/stubs "test/$n.cpp" test/stubs/ArduinoStubs.cpp       "$B/mock/src/display/Display.cpp" -o "$B/${n}_hires" 2>"$B/${n}_hires.log" || { echo "$n (DISPLAY_HIRES): BUILD FAILED"; head -5 "$B/${n}_hires.log"; status=1; continue; }
  out=$("$B/${n}_hires"); rc=$?
  printf "%s (DISPLAY_HIRES): %s\n" "$n" "$(printf '%s\n' "$out" | tail -3)"
  [ $rc -eq 0 ] || status=1
done
exit $status
