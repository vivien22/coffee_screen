/*
  GC9A01 coffee screen: icons + values (temperature, volume, duration)
  Arduino Nano + Adafruit_GC9A01A
  (also requires the Adafruit GFX Library and Adafruit BusIO)

  Wiring (Nano hardware SPI):
    GC9A01   Nano
    VCC   -> 3.3V
    GND   -> GND
    SCL   -> D13 (SCK)
    SDA   -> D11 (MOSI)
    CS    -> D10
    DC    -> D7
    RES   -> D8
    BL    -> D9   (backlight)

  Warning: the Nano outputs 5V while the GC9A01 runs at 3.3V.
  Use a level shifter or series resistors (1k) on SCL, SDA, CS, DC, RES.

  Files:
    coffee_screen.ino  this file: parameters, push events and main loop
    coffee_screen.h    encoder pins, settings, EEPROM addresses
    display.h / .cpp   drawing on the screen (pins of the screen, types, draw functions)
    icons.h            positions, colors and icons (used by display.cpp)
    fonts.h            fonts (used by display.cpp)
    CJMCU111.h / .cpp  rotary encoder with push button

  Controls:
    rotation       adjusts the selected parameter
    short push     start / stop the duration counter
    double push    select the next parameter (it gets the large digits)
    long push      store the selected parameter in the EEPROM

  Naming conventions (all identifiers in English):
    macros and constants: UPPER_SNAKE_CASE
    functions and variables: camelCase
    types (enum, struct): PascalCase
*/

#include <EEPROM.h>
#include "coffee_screen.h"
#include "display.h"
#include "CJMCU111.h"

//#define SERIAL_DEBUG   // uncomment to print the push events on the serial port (115200 bauds)
#ifdef SERIAL_DEBUG
  #define DEBUG_PRINT(text) Serial.print(F(text "\n"))
#else
  #define DEBUG_PRINT(text)
#endif

// Rotary encoder with push button; it reports the rotation and short / double / long pushes
CJMCU111 encoder(PIN_ENCODER_GA, PIN_ENCODER_GB, LONG_PUSH_MS, DOUBLE_PUSH_MS);

// Parameters shown at startup (value, size of the digits, maximum value).
// The values are replaced by the ones stored in the EEPROM in setup().
ScreenParameters parameters = {
  { 90, TEXT_SIZE_SMALL, TEMPERATURE_MAX },   // temperature (°C)
  { 60, TEXT_SIZE_LARGE, VOLUME_MAX },        // volume (ml)
  {  0, TEXT_SIZE_SMALL, DURATION_MAX }       // duration (s)
};

// Parameter adjusted by the encoder (the one with the large digits)
ActiveParameter activeParameter = PARAMETER_VOLUME;

// Duration counter
unsigned long previousMillis = 0;   // time of the last counted second
bool counting = false;              // true while the counter runs

// ------------------------------------------------------------
// Short push: start / stop the duration counter.
// A short push is reported after the double push delay, so the counter is timed from
// the moment the button was pressed (encoder.eventPressTime()), not from now.
void onShortPush() 
{
  DEBUG_PRINT("onShortPush");

  if (counting == false) {
    // Start: the next second ends one second after the press
    counting = true;
    previousMillis = encoder.eventPressTime();
    DEBUG_PRINT("Start");
  } else {
    // Stop
    counting = false;

    // Give back the seconds counted after the press (at most 2: push + delay < 2 s)
    unsigned long pressMs = encoder.eventPressTime();
    int undoneSeconds = 0;
    while ((long)(previousMillis - pressMs) > 0 && undoneSeconds < 2) {
      previousMillis -= 1000;
      undoneSeconds++;
    }

    // Reset to default value
    parameters.duration.value = EEPROM.read(PARAMETERS_DURATION_ADDR);
    drawDuration(parameters.duration);
    DEBUG_PRINT("Stop");
  }
}

// Double push: select the next parameter (it gets the large digits).
// The order is volume -> duration -> temperature -> volume.
void onDoublePush() 
{
  if (activeParameter == PARAMETER_VOLUME) {
    parameters.temperature.size = TEXT_SIZE_SMALL;
    parameters.volume.size      = TEXT_SIZE_SMALL;
    parameters.duration.size    = TEXT_SIZE_LARGE;
    activeParameter             = PARAMETER_DURATION;
  } else if (activeParameter == PARAMETER_DURATION) {
    parameters.temperature.size = TEXT_SIZE_LARGE;
    parameters.volume.size      = TEXT_SIZE_SMALL;
    parameters.duration.size    = TEXT_SIZE_SMALL;
    activeParameter = PARAMETER_TEMPERATURE;
  } else if (activeParameter == PARAMETER_TEMPERATURE) {
    parameters.temperature.size = TEXT_SIZE_SMALL;
    parameters.volume.size      = TEXT_SIZE_LARGE;
    parameters.duration.size    = TEXT_SIZE_SMALL;
    activeParameter             = PARAMETER_VOLUME;
  }

  // The sizes changed: draw the three parameters again
  drawParameters(parameters);
}

// Long push: reported as soon as the button has been held for LONG_PUSH_MS (before the release).
// Store the selected parameter in the EEPROM.
void onLongPush() 
{
  previousMillis = millis();   // while counting, the next second starts from the reset

  if (activeParameter == PARAMETER_VOLUME) {
    EEPROM.update(PARAMETERS_VOLUME_ADDR, parameters.volume.value);
  } else if (activeParameter == PARAMETER_DURATION) {
    EEPROM.update(PARAMETERS_DURATION_ADDR, parameters.duration.value);
  } else if (activeParameter == PARAMETER_TEMPERATURE) {
    EEPROM.update(PARAMETERS_TEMPERATURE_ADDR, parameters.temperature.value);
  }
}

// ------------------------------------------------------------
void setup() 
{
#ifdef SERIAL_DEBUG
  Serial.begin(115200);
#endif

  // Values stored in the EEPROM
  parameters.temperature.value = EEPROM.read(PARAMETERS_TEMPERATURE_ADDR);
  parameters.volume.value = EEPROM.read(PARAMETERS_VOLUME_ADDR);
  parameters.duration.value = EEPROM.read(PARAMETERS_DURATION_ADDR);

  beginDisplay();
  drawIcons();
  drawParameters(parameters);

  encoder.begin();
}

void loop() 
{
  // Duration counter: one step per second
  unsigned long currentMillis = millis();
  if (counting) {
    if (currentMillis - previousMillis >= 1000) {
      previousMillis += 1000;
      parameters.duration.value--;
      if (parameters.duration.value > parameters.duration.maxValue) {
        parameters.duration.value = 0;
      }
      drawDuration(parameters.duration);   // draw after counting: 1 is shown after one second
    }
  }

  // Encoder: rotation steps and push event since the last call
  byte leftSteps, rightSteps;
  PushEvent pushEvent;
  if (encoder.update(leftSteps, rightSteps, pushEvent)) {

    // Rotation: adjust the selected parameter and draw it if it changed
    int delta = (rightSteps - leftSteps) * ENCODER_STEP;
    if (activeParameter == PARAMETER_VOLUME) {
      if (parameters.volume.adjust(delta)) drawVolume(parameters.volume);
    } else if (activeParameter == PARAMETER_DURATION) {
      if (parameters.duration.adjust(delta)) drawDuration(parameters.duration);
    } else if (activeParameter == PARAMETER_TEMPERATURE) {
      if (parameters.temperature.adjust(delta)) drawTemperature(parameters.temperature);
    }

    // Push event
    if (pushEvent == PUSH_SHORT) {
      onShortPush();
    } else if (pushEvent == PUSH_DOUBLE) {
      onDoublePush();
    } else if (pushEvent == PUSH_LONG) {
      onLongPush();
    }
  }
}
