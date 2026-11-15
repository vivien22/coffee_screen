/*
  flow_sensor: the flow sensor used by the machine. FlowSensor is the real USN_FlowSensor
  (two flow sensors on A0 / A1), or the FlowSimulator when FLOW_SIMULATOR is defined in config.h.
*/

#pragma once
#include "../config.h"

#ifdef FLOW_SIMULATOR
#include "FlowSimulator.h"
typedef FlowSimulator FlowSensor;
#else
#include "USN_FlowSensor.h"
typedef USN_FlowSensor FlowSensor;
#endif
