/*
  GC9A01 coffee screen
  Arduino Nano + Adafruit_GC9A01A

  Main loop refactored into non-blocking, independent tasks:
    - encoder / button events
    - extraction timer and volume estimate
    - MAX31865 temperature acquisition
    - PID computation and time-proportioned relay
    - display updates only when needed

  Existing project dependencies and display functions are preserved.
*/

#include <Adafruit_MAX31865.h>
#include <EEPROM.h>
#include <PID_v1.h>
#include "coffee_screen.h"
#include "display.h"
#include "CJMCU111.h"
#include "volume.h"

//#define SERIAL_DEBUG
#ifdef SERIAL_DEBUG
  #define DEBUG_PRINT(text) Serial.print(text)
#else
  #define DEBUG_PRINT(text)
#endif

// -----------------------------------------------------------------------------
// Hardware
// -----------------------------------------------------------------------------

#define PID_RELAY_PIN    5
#define MAX31865_CS_PIN  6
#define PUMP_RELAY_PIN   4

// MAX31865
#define RREF      428.0
#define RNOMINAL  100.0

// -----------------------------------------------------------------------------
// Scheduling
// -----------------------------------------------------------------------------

constexpr unsigned long SENSOR_INTERVAL_MS     = 250;
constexpr unsigned long EXTRACTION_TICK_MS     = 1000;
constexpr unsigned long VOLUME_UPDATE_MS       = 200;
constexpr unsigned long TEMPERATURE_DISPLAY_MS = 1000;
constexpr int PID_WINDOW_MS                    = 1000;

// -----------------------------------------------------------------------------
// PID
// -----------------------------------------------------------------------------

double Setpoint = 0.0;
double Input = 0.0;
double Output = 0.0;

double Kp = 2.0;
double Ki = 5.0;
double Kd = 1.0;

PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);
unsigned long pidWindowStart = 0;

// -----------------------------------------------------------------------------
// Devices and application data
// -----------------------------------------------------------------------------

Adafruit_MAX31865 thermo = Adafruit_MAX31865(MAX31865_CS_PIN);

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

bool counting = false;
bool temperatureSensorValid = false;
bool previousRelayState = false;
bool relayIndicatorInitialized = false;

unsigned long extractionSecondAt = 0;
unsigned long volumeUpdateAt = 0;
unsigned long sensorReadAt = 0;
unsigned long temperatureDisplayAt = 0;

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------

void handleInputs();
void handleEncoderRotation(byte leftSteps, byte rightSteps);
void updateExtractionTimer(unsigned long now);
void updateExtractionVolume(unsigned long now);
void updateTemperatureSensor(unsigned long now);
void updateTemperatureDisplay(unsigned long now);
void updateTemperatureControl(unsigned long now);
void startExtraction();
void stopExtraction();

// -----------------------------------------------------------------------------
// Extraction lifecycle
// -----------------------------------------------------------------------------

void startExtraction()
{
  if (counting) {
    return;
  }

  const unsigned long now = millis();

  counting = true;
  extractionSecondAt = encoder.eventPressTime();
  volumeUpdateAt = now;

  digitalWrite(PUMP_RELAY_PIN, HIGH);

  DEBUG_PRINT("Extraction started\n");
}

void stopExtraction()
{
  if (!counting) {
    return;
  }

  counting = false;
  digitalWrite(PUMP_RELAY_PIN, LOW);

  // Keep the final measured volume visible until the next extraction.
  drawVolume(parameters.volume, parameters.volume.currentValue);

  // Reset the counters for the next extraction.
  parameters.volume.currentValue = parameters.volume.defaultValue;
  parameters.duration.currentValue = 0;

  drawDuration(parameters.duration, parameters.duration.currentValue);

  DEBUG_PRINT("Extraction stopped\n");
}

void onShortPush()
{
  if (counting) {
    stopExtraction();
  } else if(activeParameter == PARAMETER_VOLUME){
    startExtraction();
  }
}

// Double push selects the next editable parameter.
// Extraction is stopped only if it is actually running.
void onDoublePush()
{
  if (counting) {
    stopExtraction();
  }

  if (activeParameter == PARAMETER_VOLUME) {
    parameters.temperature.size = TEXT_SIZE_LARGE;
    parameters.volume.size = TEXT_SIZE_SMALL;
    parameters.duration.size = TEXT_SIZE_SMALL;

    activeParameter = PARAMETER_TEMPERATURE;
    parameters.temperature.currentValue =
      parameters.temperature.defaultValue;
  } else if (activeParameter == PARAMETER_TEMPERATURE) {
    parameters.temperature.size = TEXT_SIZE_SMALL;
    parameters.volume.size = TEXT_SIZE_LARGE;
    parameters.duration.size = TEXT_SIZE_SMALL;

    activeParameter = PARAMETER_VOLUME;
  }

  drawCurrentParameters(parameters);
}

void onLongPush()
{
  // Do not modify extraction timestamps here: saving a setting must not
  // change the elapsed extraction time.
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
// Input handling
// -----------------------------------------------------------------------------

void handleEncoderRotation(byte leftSteps, byte rightSteps)
{
  const int delta =
    ((int)rightSteps - (int)leftSteps) * ENCODER_STEP;

  if (delta == 0) {
    return;
  }

  if (activeParameter == PARAMETER_VOLUME) {
    if (parameters.volume.adjust(delta)) {
      drawVolume(
        parameters.volume,
        parameters.volume.defaultValue
      );
    }
  } else if (activeParameter == PARAMETER_TEMPERATURE) {
    if (parameters.temperature.adjust(delta)) {
      drawTemperature(
        parameters.temperature,
        parameters.temperature.defaultValue
      );
    }
  }
}

void handleInputs()
{
  byte leftSteps = 0;
  byte rightSteps = 0;
  PushEvent pushEvent;

  // Call update on every loop pass so the encoder library can debounce and
  // recognize short / double / long presses with its own timing.
  if (!encoder.update(leftSteps, rightSteps, pushEvent)) {
    return;
  }

  handleEncoderRotation(leftSteps, rightSteps);

  switch (pushEvent) {
    case PUSH_SHORT:
      onShortPush();
      break;

    case PUSH_DOUBLE:
      onDoublePush();
      break;

    case PUSH_LONG:
      onLongPush();
      break;

    default:
      break;
  }
}

// -----------------------------------------------------------------------------
// Extraction tasks
// -----------------------------------------------------------------------------

void updateExtractionTimer(unsigned long now)
{
  if (!counting) {
    return;
  }

  // Use a while loop to account for delayed loop passes without losing ticks.
  while ((unsigned long)(now - extractionSecondAt) >= EXTRACTION_TICK_MS) {
    extractionSecondAt += EXTRACTION_TICK_MS;

    parameters.duration.currentValue++;

    if (parameters.duration.currentValue >
        parameters.duration.maxValue) {
      parameters.duration.currentValue = 0;
    }

    drawDuration(
      parameters.duration,
      parameters.duration.currentValue
    );
  }
}

void updateExtractionVolume(unsigned long now)
{
  if (!counting) {
    return;
  }

  if ((unsigned long)(now - volumeUpdateAt) < VOLUME_UPDATE_MS) {
    return;
  }

  // Do not replay several old display updates after a delayed loop pass.
  volumeUpdateAt = now;

  parameters.volume.currentValue =
    getVolume(parameters.duration.currentValue);

  drawVolume(
    parameters.volume,
    parameters.volume.currentValue
  );
}

// -----------------------------------------------------------------------------
// Temperature acquisition and display
// -----------------------------------------------------------------------------

void updateTemperatureSensor(unsigned long now)
{
  if ((unsigned long)(now - sensorReadAt) < SENSOR_INTERVAL_MS) {
    return;
  }

  sensorReadAt = now;

  const double measuredTemperature =
    thermo.temperature(RNOMINAL, RREF);

  // A basic plausibility check. A full RTD fault strategy should also inspect
  // thermo.readFault() and inhibit the heater on any sensor fault.
  if (isnan(measuredTemperature) ||
      measuredTemperature < -20.0 ||
      measuredTemperature > 300.0) {
    temperatureSensorValid = false;
    digitalWrite(PID_RELAY_PIN, LOW);
    return;
  }

  Input = measuredTemperature;
  temperatureSensorValid = true;
}

void updateTemperatureDisplay(unsigned long now)
{
  if (!temperatureSensorValid ||
      activeParameter == PARAMETER_TEMPERATURE) {
    return;
  }

  if ((unsigned long)(now - temperatureDisplayAt) <
      TEMPERATURE_DISPLAY_MS) {
    return;
  }

  temperatureDisplayAt = now;

  const int newValue = (int)(Input + 0.5);

  if (newValue != parameters.temperature.currentValue) {
    parameters.temperature.currentValue = newValue;

    drawTemperature(
      parameters.temperature,
      parameters.temperature.currentValue
    );
  }
}

// -----------------------------------------------------------------------------
// Temperature control
// -----------------------------------------------------------------------------

void updateTemperatureControl(unsigned long now)
{
  // Never energize the heater if the temperature reading is invalid.
  if (!temperatureSensorValid) {
    digitalWrite(PID_RELAY_PIN, LOW);

    if (!relayIndicatorInitialized || previousRelayState) {
      blinkPIDOutputIndicator(false);
      previousRelayState = false;
      relayIndicatorInitialized = true;
    }
    return;
  }

  myPID.Compute();

  // Time-proportioning relay control.
  if ((unsigned long)(now - pidWindowStart) >=
      (unsigned long)PID_WINDOW_MS) {
    // Resynchronize to now rather than iterating through missed windows.
    pidWindowStart = now;
  }

  const unsigned long elapsedInWindow = now - pidWindowStart;
  const bool relayOn =
    (Output > 0.0) &&
    (elapsedInWindow < (unsigned long)constrain(
      (int)Output, 0, PID_WINDOW_MS
    ));

  digitalWrite(PID_RELAY_PIN, relayOn ? HIGH : LOW);

  // Update the screen indicator only when the relay state changes.
  if (!relayIndicatorInitialized || relayOn != previousRelayState) {
    blinkPIDOutputIndicator(relayOn);
    previousRelayState = relayOn;
    relayIndicatorInitialized = true;
  }
}

// -----------------------------------------------------------------------------
// Arduino setup / loop
// -----------------------------------------------------------------------------

void setup()
{
#ifdef SERIAL_DEBUG
  Serial.begin(115200);
#endif

  pinMode(PID_RELAY_PIN, OUTPUT);
  pinMode(PUMP_RELAY_PIN, OUTPUT);

  // Safe initial output states.
  digitalWrite(PID_RELAY_PIN, LOW);
  digitalWrite(PUMP_RELAY_PIN, LOW);

  // Load saved settings.
  parameters.temperature.defaultValue =
    EEPROM.read(PARAMETERS_TEMPERATURE_ADDR);

  parameters.volume.defaultValue =
    EEPROM.read(PARAMETERS_VOLUME_ADDR);

  parameters.volume.currentValue =
    parameters.volume.defaultValue;

  // Initialize display and encoder.
  beginDisplay();
  encoder.begin();

  // Initialize MAX31865.
  thermo.begin(MAX31865_2WIRE);

  const double initialTemperature =
    thermo.temperature(RNOMINAL, RREF);

  if (!isnan(initialTemperature) &&
      initialTemperature >= -20.0 &&
      initialTemperature <= 300.0) {
    Input = initialTemperature;
    temperatureSensorValid = true;
    parameters.temperature.currentValue =
      (int)(initialTemperature + 0.5);
  } else {
    Input = 0.0;
    temperatureSensorValid = false;
  }

  // Initialize PID.
  Setpoint = (double)parameters.temperature.defaultValue;
  myPID.SetOutputLimits(0, PID_WINDOW_MS);
  myPID.SetSampleTime(SENSOR_INTERVAL_MS);
  myPID.SetMode(AUTOMATIC);

  const unsigned long now = millis();
  pidWindowStart = now;
  sensorReadAt = now;
  temperatureDisplayAt = now;
  extractionSecondAt = now;
  volumeUpdateAt = now;

  // Draw the interface.
  drawIcons();
  drawCurrentParameters(parameters);
}

void loop()
{
  // Read the clock once per pass so all tasks use a consistent timestamp.
  const unsigned long now = millis();

  // 1. Inputs: the encoder library is polled on every loop pass.
  handleInputs();

  // 2. Extraction: timer and volume are independent tasks.
  updateExtractionTimer(now);
  updateExtractionVolume(now);

  // 3. Temperature: acquisition, display and control are independent.
  updateTemperatureSensor(now);
  updateTemperatureDisplay(now);
  updateTemperatureControl(now);
}
