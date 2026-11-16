# Tests

Host tests of the logic (heater controller, tuning console, temperature sensor), no Arduino needed:

    ./test/run.sh

`test/stubs` replaces the Arduino libraries (`Serial` keeps what is printed and gives what the test types, `PID_v1`
is a small host version of the library), `test/mock/FlowSensor.h` replaces the flow sensor (values set by
the tests). `test_sim_*` tests use the real `FlowSimulator`.

All the tests share `test/stubs/ArduinoStubs.cpp` (the clock `t`, the pins `pins[]`, `Serial`, `SPI`)
and `test/stubs/TestSupport.h` (`CHECK` and the count of failures): include `TestSupport.h`, `run.sh` links the
stubs.
