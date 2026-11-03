/*
 * coffee_screen.ino
 * Arduino Nano (ATmega328P, 32 KB Flash / 2 KB SRAM)
 *
 * Architecture:
 *   - cooperative loop, no RTOS and no delay()
 *   - compact event flags + accumulated encoder delta
 *   - extraction finite-state machine
 *   - periodic temperature acquisition / PID
 *   - display dirty flags
 *
 * Existing project files expected:
 *   coffee_screen.h, display.h/.cpp, CJMCU111.h/.cpp,
 *   volume.h/.cpp, icons.h, fonts.h
 *
 * Important:
 *   - The flowmeter ISR is deliberately not guessed: its pin and
 *     pulses-per-ml calibration must come from the actual wiring/calibration.
 *   - Heater control is disabled on MAX31865 fault or implausible temperature.
 *     This is not a substitute for an independent thermal fuse/thermostat.
 */

#include <Arduino.h>
#include <Adafruit_MAX31865.h>
#include <EEPROM.h>
#include <PID_v1.h>

#include "coffee_screen.h"
#include "display.h"
#include "CJMCU111.h"
#include "USN_FlowSensor.h"
#include "coffee_events.h"

//#define SERIAL_DEBUG

#ifdef SERIAL_DEBUG
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) \
  do { \
  } while (0)
#define DEBUG_PRINTLN(...) \
  do { \
  } while (0)
#endif

// -----------------------------------------------------------------------------
// Hardware configuration
// -----------------------------------------------------------------------------

#define PID_RELAY_PIN    5
#define MAX31865_CS_PIN  6
#define PUMP_RELAY_PIN   4

#define RREF                  428.0
#define RNOMINAL              100.0
#define VALID_TEMPERATURE_MIN 0
#define VALID_TEMPERATURE_MAX 199

// Change to MAX31865_3WIRE or MAX31865_4WIRE if required by your sensor wiring.
#define RTD_WIRING MAX31865_2WIRE

// -----------------------------------------------------------------------------
// Timing
// -----------------------------------------------------------------------------

static const uint16_t SENSOR_PERIOD_MS       = 250;
static const uint16_t VOLUME_PERIOD_MS       = 200;
static const uint16_t DURATION_DISPLAY_MS    = 200;
static const uint16_t TEMPERATURE_DISPLAY_MS = 1000;
static const int      PID_WINDOW_MS          = 1000;

// -----------------------------------------------------------------------------
// Feedforward
// -----------------------------------------------------------------------------
// While water leaves the boiler, the same amount of cold water enters it. Heating that
// water from WATER_INLET_TEMPERATURE to the setpoint takes
//     power [W] = flow [ml/s] x 4.186 J/(g.K) x (setpoint - inlet temperature) [K]    (1 ml = 1 g)
// The heater is switched on for the matching part of the PID window as soon as the water
// flows, BEFORE the sensor sees the temperature drop. The PID only corrects what the
// feedforward does not cover.

#define HEATER_POWER_W           1200.0f   // power of the heating element (W): set it to your boiler
#define WATER_INLET_TEMPERATURE  22.0f     // temperature of the water entering the boiler (degrees C)
#define WATER_SPECIFIC_HEAT      4.186f    // J/(g.K)
#define FEEDFORWARD_GAIN         10.0f     // 0 = feedforward off, 1 = full estimate; tune on a real extraction

// -----------------------------------------------------------------------------
// PID
// -----------------------------------------------------------------------------

// The PID will be activated only if the temperature is at least PID_WORKING_RANGE from the Setpoint
#define PID_WORKING_RANGE 20

double Setpoint = 0.0;
double Input = 0.0;
double Output = 0.0;

double Kp = 35;
double Ki = 0.05;
double Kd = 180;

PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);
uint32_t pidWindowStart = 0;
bool PIDModeChanged = false;

// Part of the PID window (ms) given by the feedforward; the PID output is added to it
int feedforwardMs = 0;

// -----------------------------------------------------------------------------
// Devices and application data
// -----------------------------------------------------------------------------

Adafruit_MAX31865 thermo(MAX31865_CS_PIN);

USN_FlowSensor systemFlow;

CJMCU111 encoder(
  PIN_ENCODER_GA,
  PIN_ENCODER_GB,
  LONG_PUSH_MS,
  DOUBLE_PUSH_MS
);

ScreenParameters parameters = {
  { 0, 0, TEXT_SIZE_SMALL, TEMPERATURE_MAX },
  { 0, 0, TEXT_SIZE_LARGE, VOLUME_MAX },
  { 0, 0, TEXT_SIZE_SMALL, DURATION_MAX }
};

ActiveParameter activeParameter = PARAMETER_VOLUME;

enum ExtractionState : uint8_t {
  EXTRACTION_IDLE,
  EXTRACTION_RUNNING,
  EXTRACTION_FINISHED,
  EXTRACTION_ERROR
};

ExtractionState extractionState = EXTRACTION_IDLE;

bool temperatureValid = false;
bool heaterState = false;
bool heaterIndicatorInitialized = false;

uint32_t extractionStartedAt = 0;
uint32_t lastVolumeUpdateAt = 0;
uint32_t lastDurationDrawAt = 0;
uint32_t lastSensorReadAt = 0;
uint32_t lastTemperatureDrawAt = 0;

enum DisplayDirty : uint8_t {
  DISPLAY_CLEAN       = 0,
  DISPLAY_VOLUME      = 1 << 0,
  DISPLAY_TEMPERATURE = 1 << 1,
  DISPLAY_DURATION    = 1 << 2,
  DISPLAY_ALL         = 0x07
};

uint8_t displayDirty = DISPLAY_ALL;

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------

static void pollInputs();
static void processPendingEvents();
static void applyEncoderDelta(int16_t delta);
static void startExtraction(uint32_t eventTime);
static void stopExtraction(ExtractionState finalState);
static void updateExtraction(uint32_t now);
static void updateTemperatureSensor(uint32_t now);
static void updateFlowSensor();
static void updateTemperatureControl(uint32_t now);
static int  computeFeedforwardMs(uint32_t now);
static void updateDisplay();
static void selectNextParameter();
static void saveSelectedParameter();
static void setHeater(bool on);

// -----------------------------------------------------------------------------
// Input acquisition and event dispatch
// -----------------------------------------------------------------------------

static void pollInputs()
{
  // Must be polled on every loop pass for correct debounce / click timing.
  byte leftSteps = 0;
  byte rightSteps = 0;
  PushEvent pushEvent;

  if (!encoder.update(leftSteps, rightSteps, pushEvent)) {
    return;
  }

  const int16_t delta =
    ((int16_t)rightSteps - (int16_t)leftSteps) * ENCODER_STEP;

  if (delta != 0) {
    addEncoderDelta(delta);
    postCoffeeEvent(COFFEE_EVENT_ENCODER);
  }

  switch (pushEvent) {
    case PUSH_SHORT:
      postCoffeeEvent(COFFEE_EVENT_SHORT);
      break;

    case PUSH_DOUBLE:
      postCoffeeEvent(COFFEE_EVENT_DOUBLE);
      break;

    case PUSH_LONG:
      postCoffeeEvent(COFFEE_EVENT_LONG);
      break;

    default:
      break;
  }
}

static void processPendingEvents()
{
  const int16_t delta = takeEncoderDelta();
  const uint8_t pending = takeCoffeeEvents();

  if (delta != 0) {
    applyEncoderDelta(delta);
  }

  // Process at most one event of each type per pass. The encoder library
  // emits at most one push classification per update call.
  if (pending & COFFEE_EVENT_SHORT) {
    if (extractionState == EXTRACTION_RUNNING) {
      stopExtraction(EXTRACTION_FINISHED);
    } else if(activeParameter == PARAMETER_VOLUME) {
      startExtraction(encoder.eventPressTime());
    }
  }

  if (pending & COFFEE_EVENT_DOUBLE) {
    if (extractionState == EXTRACTION_RUNNING) {
      stopExtraction(EXTRACTION_FINISHED);
    }
    selectNextParameter();
  }

  if (pending & COFFEE_EVENT_LONG) {
    saveSelectedParameter();
  }
}

static void applyEncoderDelta(int16_t delta)
{
  if (activeParameter == PARAMETER_VOLUME) {
    if (parameters.volume.adjust(delta)) {
      displayDirty |= DISPLAY_VOLUME;
    }
  } else if (activeParameter == PARAMETER_TEMPERATURE) {
    if (parameters.temperature.adjust(delta)) {
      displayDirty |= DISPLAY_TEMPERATURE;
    }
  }
}

// -----------------------------------------------------------------------------
// Parameter selection and persistence
// -----------------------------------------------------------------------------

static void selectNextParameter()
{
  if (activeParameter == PARAMETER_VOLUME) {
    parameters.temperature.size = TEXT_SIZE_LARGE;
    parameters.volume.size = TEXT_SIZE_SMALL;
    parameters.duration.size = TEXT_SIZE_SMALL;

    activeParameter = PARAMETER_TEMPERATURE;
    parameters.temperature.currentValue =
      parameters.temperature.defaultValue;
  } else {
    parameters.temperature.size = TEXT_SIZE_SMALL;
    parameters.volume.size = TEXT_SIZE_LARGE;
    parameters.duration.size = TEXT_SIZE_SMALL;

    activeParameter = PARAMETER_VOLUME;
    parameters.volume.currentValue =
      parameters.volume.defaultValue;
  }

  displayDirty |= DISPLAY_ALL;
}

static void saveSelectedParameter()
{
  if (activeParameter == PARAMETER_VOLUME) {
    EEPROM.update(
      PARAMETERS_VOLUME_ADDR,
      parameters.volume.defaultValue
    );
    blinkVolume(parameters.volume);
  } else if (activeParameter == PARAMETER_TEMPERATURE) {
    EEPROM.update(
      PARAMETERS_TEMPERATURE_ADDR,
      parameters.temperature.defaultValue
    );

    Setpoint = (double)parameters.temperature.defaultValue;
    blinkTemperature(parameters.temperature);
  }
}

// -----------------------------------------------------------------------------
// Extraction state machine
// -----------------------------------------------------------------------------

static void startExtraction(uint32_t eventTime)
{
  if (extractionState == EXTRACTION_RUNNING) {
    return;
  }

  extractionState = EXTRACTION_RUNNING;
  extractionStartedAt = eventTime;
  systemFlow.startExtraction();

  // If event timestamp is not usable, start timing from now.
  const uint32_t now = millis();
  if ((uint32_t)(now - extractionStartedAt) > 5000UL) {
    extractionStartedAt = now;
  }

  parameters.duration.currentValue = 0;
  parameters.volume.currentValue = parameters.volume.defaultValue;

  lastVolumeUpdateAt = now;
  lastDurationDrawAt = now;

  digitalWrite(PUMP_RELAY_PIN, HIGH);
  displayDirty |= DISPLAY_VOLUME | DISPLAY_DURATION;

  DEBUG_PRINTLN(F("Extraction started"));
}

static void stopExtraction(ExtractionState finalState)
{
  if (extractionState != EXTRACTION_RUNNING) {
    return;
  }

  extractionState = finalState;
  systemFlow.stopExtraction();
  digitalWrite(PUMP_RELAY_PIN, LOW);

  // Update elapsed time and estimated volume once at stop.
  const uint32_t elapsedSeconds =
    (uint32_t)(millis() - extractionStartedAt) / 1000UL;

  parameters.duration.currentValue = (int)elapsedSeconds;
  parameters.volume.currentValue = systemFlow.getNetVolume();

  displayDirty |= DISPLAY_VOLUME | DISPLAY_DURATION;

  DEBUG_PRINTLN(F("Extraction stopped"));
}

static void updateExtraction(uint32_t now)
{
  if (extractionState != EXTRACTION_RUNNING) {
    return;
  }

  const uint32_t elapsedMs = (uint32_t)(now - extractionStartedAt);
  const uint32_t elapsedSeconds = elapsedMs / 1000UL;

  if ((uint32_t)(now - lastDurationDrawAt) >= DURATION_DISPLAY_MS) {
    lastDurationDrawAt = now;

    const int newDuration = (int)elapsedSeconds;
    if (newDuration != parameters.duration.currentValue) {
      parameters.duration.currentValue = newDuration;
      displayDirty |= DISPLAY_DURATION;
    }
  }

  if ((uint32_t)(now - lastVolumeUpdateAt) >= VOLUME_PERIOD_MS) {
    lastVolumeUpdateAt = now;

    const int newVolume = (int)(systemFlow.getOutflowVolume() + 0.5f);
    if (newVolume != parameters.volume.currentValue) {
      parameters.volume.currentValue = newVolume;
      displayDirty |= DISPLAY_VOLUME;
      if (parameters.volume.currentValue >= parameters.volume.defaultValue) {
        stopExtraction(EXTRACTION_FINISHED);
      }
    }
  }

  // Optional maximum extraction duration guard. Uses the existing duration
  // maximum from the screen configuration as a conservative software limit.
  if (elapsedSeconds > (uint32_t)parameters.duration.maxValue) {
    stopExtraction(EXTRACTION_ERROR);
  }
}

// -----------------------------------------------------------------------------
// Flow sensor acquisition
// -----------------------------------------------------------------------------

static void updateFlowSensor()
{
  systemFlow.update();
}

// -----------------------------------------------------------------------------
// Temperature acquisition and heater control
// -----------------------------------------------------------------------------

static void updateTemperatureSensor(uint32_t now)
{
  if ((uint32_t)(now - lastSensorReadAt) < SENSOR_PERIOD_MS) {
    return;
  }

  lastSensorReadAt = now;

  const uint8_t fault = thermo.readFault();
  if (fault != 0) {
    thermo.clearFault();
    temperatureValid = false;
    setHeater(false);
    DEBUG_PRINT(F("MAX31865 fault: 0x"));
    DEBUG_PRINTLN(fault, HEX);
    return;
  }

  const double measured = thermo.temperature(RNOMINAL, RREF);

  if (isnan(measured) || measured < VALID_TEMPERATURE_MIN || measured > VALID_TEMPERATURE_MAX) {
    temperatureValid = false;
    setHeater(false);
    return;
  }

  Input = measured;
  temperatureValid = true;

  if (activeParameter != PARAMETER_TEMPERATURE && (uint32_t)(now - lastTemperatureDrawAt) >= TEMPERATURE_DISPLAY_MS) {
    lastTemperatureDrawAt = now;

    const int roundedTemperature = (int)(measured + 0.5);
    if (roundedTemperature != parameters.temperature.currentValue) {
      parameters.temperature.currentValue = roundedTemperature;
      displayDirty |= DISPLAY_TEMPERATURE;
    }
  }
}

// Heater on-time (ms per PID window) needed to heat the water that leaves the boiler.
// 0 outside of an extraction.
static int computeFeedforwardMs(uint32_t now)
{
  if (extractionState != EXTRACTION_RUNNING) {
    return 0;
  }

  // USN_FlowSensor returns mL/min in both physical and simulation modes.
  const float flow = systemFlow.getOutflowRate() / 60.0f;           // mL/s
  const float heating = (float)Setpoint - WATER_INLET_TEMPERATURE;  // degrees to give to the new water

  if (flow <= 0.0f || heating <= 0.0f) {
    return 0;
  }

  const float powerW = flow * WATER_SPECIFIC_HEAT * heating * FEEDFORWARD_GAIN;
  const float ms = powerW / HEATER_POWER_W * (float)PID_WINDOW_MS;

  return constrain((int)(ms + 0.5f), 0, PID_WINDOW_MS);
}

static void updateTemperatureControl(uint32_t now) {
  if (!temperatureValid) {
    feedforwardMs = 0;
    setHeater(false);
    DEBUG_PRINTLN("temperatureInvalid");
    return;
  }

  if (parameters.temperature.defaultValue - parameters.temperature.currentValue > PID_WORKING_RANGE){
    myPID.SetMode(MANUAL);
    Output = PID_WINDOW_MS;
    PIDModeChanged = true;
  }else{
    if(PIDModeChanged){
      Output = 0;
      PIDModeChanged = false;
    }
    // The PID works around the feedforward: it can add to it or take from it, but the total
    // always stays between 0 and the window (so the PID integral cannot wind up).
    feedforwardMs = computeFeedforwardMs(now);

    myPID.SetMode(AUTOMATIC);
    myPID.SetOutputLimits(-feedforwardMs, PID_WINDOW_MS - feedforwardMs);
    myPID.Compute();
  }

  if ((uint32_t)(now - pidWindowStart) >= (uint32_t)PID_WINDOW_MS) {
    pidWindowStart = now;
  }

  const uint32_t elapsedInWindow = (uint32_t)(now - pidWindowStart);
  const int outputMs = constrain((int)Output + feedforwardMs, 0, PID_WINDOW_MS);
  const bool requestedState = elapsedInWindow < (uint32_t)outputMs;

  setHeater(requestedState);
}

static void setHeater(bool on)
{
  if (on == heaterState && heaterIndicatorInitialized) {
    return;
  }

  heaterState = on;
  heaterIndicatorInitialized = true;

  digitalWrite(PID_RELAY_PIN, on ? HIGH : LOW);
  blinkPIDOutputIndicator(on);
}

// -----------------------------------------------------------------------------
// Display: render only fields marked dirty
// -----------------------------------------------------------------------------

static void updateDisplay()
{
  const uint8_t dirty = displayDirty;
  if (dirty == DISPLAY_CLEAN) {
    return;
  }

  displayDirty = DISPLAY_CLEAN;

  if (dirty == DISPLAY_ALL) {
    drawCurrentParameters(parameters);
    return;
  }

  if (dirty & DISPLAY_VOLUME) {
    const int valueToDraw =
      (extractionState == EXTRACTION_RUNNING)
        ? parameters.volume.currentValue
        : parameters.volume.defaultValue;

    drawVolume(parameters.volume, valueToDraw);
  }

  if (dirty & DISPLAY_TEMPERATURE) {
    const int valueToDraw =
      (activeParameter == PARAMETER_TEMPERATURE)
        ? parameters.temperature.defaultValue
        : parameters.temperature.currentValue;

    drawTemperature(parameters.temperature, valueToDraw);
  }

  if (dirty & DISPLAY_DURATION) {
    drawDuration(
      parameters.duration,
      parameters.duration.currentValue
    );
  }
}

// -----------------------------------------------------------------------------
// Arduino entry points
// -----------------------------------------------------------------------------

void setup()
{
#ifdef SERIAL_DEBUG
  Serial.begin(115200);
#endif

  pinMode(PID_RELAY_PIN, OUTPUT);
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  digitalWrite(PID_RELAY_PIN, LOW);
  digitalWrite(PUMP_RELAY_PIN, LOW);

  // Load persistent settings.
  parameters.temperature.defaultValue =
    EEPROM.read(PARAMETERS_TEMPERATURE_ADDR);
  parameters.volume.defaultValue =
    EEPROM.read(PARAMETERS_VOLUME_ADDR);

  parameters.volume.currentValue =
    parameters.volume.defaultValue;

  beginDisplay();
  encoder.begin();

  thermo.begin(RTD_WIRING);

  systemFlow.begin();

  // First sensor read, with fault check before enabling the PID.
  const uint8_t initialFault = thermo.readFault();
  if (initialFault == 0) {
    const double initialTemperature = thermo.temperature(RNOMINAL, RREF);

    if (!isnan(initialTemperature) && initialTemperature >= VALID_TEMPERATURE_MIN && initialTemperature <= VALID_TEMPERATURE_MAX) {
      Input = initialTemperature;
      temperatureValid = true;
      parameters.temperature.currentValue =
        (int)(initialTemperature + 0.5);
    }
  } else {
    thermo.clearFault();
  }

  Setpoint = (double)parameters.temperature.defaultValue;

  myPID.SetOutputLimits(0, PID_WINDOW_MS);
  myPID.SetSampleTime(SENSOR_PERIOD_MS);
  myPID.SetMode(AUTOMATIC);

  const uint32_t now = millis();
  pidWindowStart = now;
  lastSensorReadAt = now;
  lastTemperatureDrawAt = now;
  lastVolumeUpdateAt = now;
  lastDurationDrawAt = now;

  drawIcons();
  drawCurrentParameters(parameters);

  // If the sensor was invalid at startup, keep the heater off.
  if (!temperatureValid) {
    setHeater(false);
  }
}

void loop()
{
  // Poll the encoder every pass; its debounce/click state machine needs this.
  pollInputs();

  // Dispatch user actions and accumulated encoder movement.
  processPendingEvents();

  // Use one timestamp for the periodic tasks in this pass.
  const uint32_t now = millis();

  updateExtraction(now);
  updateFlowSensor();
  updateTemperatureSensor(now);
  updateTemperatureControl(now);
  updateDisplay();
}
