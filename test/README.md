# Tests

Host tests of the logic (state machine, settings, screen controller), no Arduino needed:

    ./test/run.sh

`test/stubs` replaces the Arduino libraries, `test/mock/FlowSensor.h` replaces the flow sensor
(values set by the tests). `test_sim_*` tests use the real `FlowSimulator`.

All the tests share `test/stubs/ArduinoStubs.cpp` (the clock `t`, the pins `pins[]`, `Serial`, `SPI`, `EEPROM`)
and `test/stubs/TestSupport.h` (`CHECK` and the count of failures): include `TestSupport.h`, `run.sh` links the
stubs.
