#!/bin/sh
# Host tests of the logic (no Arduino needed): ./test/run.sh
# The sources are copied to a build folder. Three flavours (DRIVE_PUMP_AND_VALVE is switched on in the first two, which test
# the extraction; the default Config.h has the boiler only):
#   test_*.cpp      the flow sensor is replaced by test/mock/FlowSensor.h (values set by the test)
#   test_sim_*.cpp  the real FlowSimulator (FLOW_SIMULATOR, switched on in the copy of Config.h) follows the pump relay
#   test_boiler_only.cpp  the Config.h as it is: the heater only, the pump and the valve never go on
set -e
cd "$(dirname "$0")/.."
B=${TMPDIR:-/tmp}/coffee_screen_test
rm -rf "$B" && mkdir -p "$B/mock" "$B/real" "$B/boiler"
for d in mock real boiler; do
  cp -r src "$B/$d/src"
  cp test/stubs/EEPROM.h "$B/$d/"
done
cp test/mock/FlowSensor.h "$B/mock/src/drivers/FlowSensor.h"
cp test/mock/FlowSensor.h "$B/boiler/src/drivers/FlowSensor.h"
# DRIVE_PUMP_AND_VALVE is commented out in Config.h (the boiler only): the copies for the extraction tests switch it on
sed -i 's|^// #define DRIVE_PUMP_AND_VALVE|#define DRIVE_PUMP_AND_VALVE|' "$B/mock/src/Config.h" "$B/real/src/Config.h"
# FLOW_SIMULATOR is commented out in Config.h (the real flow sensors): the copy of the test_sim_* flavour switches it on
sed -i 's|^// #define FLOW_SIMULATOR|#define FLOW_SIMULATOR|' "$B/real/src/Config.h"
status=0
for t in test/test_*.cpp; do
  n=$(basename "$t" .cpp)
  case "$n" in test_sim_*) d=real;; test_boiler_only) d=boiler;; *) d=mock;; esac
  # sources linked with the test (the test provides the stubs of what it does not link)
  C="$B/$d/src/machine/ExtractionController.cpp"
  case "$n" in
    test_flush|test_pre) C="$C $B/$d/src/settings/Settings.cpp $B/$d/src/display/ScreenController.cpp";;
    test_temperature) C="$B/$d/src/heating/TemperatureSensor.cpp";;
    test_encoder) C="$B/$d/src/drivers/CJMCU111.cpp";;
    test_display|test_welcome) C="$B/$d/src/display/Display.cpp";;
    test_machine|test_boiler_only) C="$C $B/$d/src/settings/Settings.cpp $B/$d/src/display/ScreenController.cpp $B/$d/src/machine/CoffeeMachine.cpp $B/$d/src/heating/HeaterController.cpp $B/$d/src/heating/TemperatureSensor.cpp $B/$d/src/drivers/CJMCU111.cpp";;
  esac
  g++ -std=c++11 -w -I"$B/$d" -Itest/stubs "$t" test/stubs/ArduinoStubs.cpp \
      $C -o "$B/$n" 2>"$B/$n.log" || { echo "$n: BUILD FAILED"; head -5 "$B/$n.log"; status=1; continue; }
  printf "%s: " "$n"; "$B/$n" | tail -3 || status=1
done
exit $status
