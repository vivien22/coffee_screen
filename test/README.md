# Tests

Host tests of the logic (state machine, settings, screen controller), no Arduino needed:

    ./test/run.sh

`test/stubs` replaces the Arduino libraries, `test/mock/flow_sensor.h` replaces the flow sensor
(values set by the tests). `test_sim_*` tests use the real `FlowSimulator`.
