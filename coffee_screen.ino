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

#include <Adafruit_MAX31865.h>
#include <EEPROM.h>
#include <PID_v1.h>
#include "coffee_screen.h"
#include "display.h"
#include "CJMCU111.h"
#include "volume.h"

//#define SERIAL_DEBUG   // uncomment to print the push events on the serial port (115200 bauds)
#ifdef SERIAL_DEBUG
  #define DEBUG_PRINT(text) Serial.print(text)
#else
  #define DEBUG_PRINT(text)
#endif

// PID parameters
#define PIN_INPUT 0
#define RELAY_PIN 6

//Define Variables we'll be connecting to
double Setpoint, Input, Output;

//Specify the links and initial tuning parameters
double Kp=2, Ki=5, Kd=1;
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);

int WindowSize = 1000;
unsigned long windowStartTime;

// MAX31865 parameters
// use hardware SPI, just pass in the CS pin
Adafruit_MAX31865 thermo = Adafruit_MAX31865(10);

// The value of the Rref resistor. Use 430.0 for PT100 and 4300.0 for PT1000
#define RREF      430.0
// The 'nominal' 0-degrees-C resistance of the sensor
#define RNOMINAL  100.0

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
unsigned long previousMillis200 = 0;
bool counting = false;              // true while the counter runs

// ------------------------------------------------------------

void startExtraction()
{
  // Start: the next second ends one second after the press
    counting = true;
    previousMillis = encoder.eventPressTime();
    DEBUG_PRINT("Start");
} 

void stopExtraction()
{
  // Stop
    counting = false;

    // Give back the seconds counted after the press (at most 2: push + delay < 2 s)
    unsigned long pressMs = encoder.eventPressTime();
    int undoneSeconds = 0;
    while ((long)(previousMillis - pressMs) > 0 && undoneSeconds < 2) {
      previousMillis -= 1000;
      undoneSeconds++;
    }

    drawVolume(parameters.volume);
    // Reset to default value
    parameters.volume.value = EEPROM.read(PARAMETERS_VOLUME_ADDR);
    drawDuration(parameters.duration);
    parameters.duration.value = 0;
    DEBUG_PRINT("Stop");
} 

void onShortPush() 
{
  DEBUG_PRINT("onShortPush");

  if (counting == false) {
    startExtraction();
  } else {
    stopExtraction();
  }
}

// Double push: select the next parameter (it gets the large digits).
// The order is volume -> duration -> temperature -> volume.
void onDoublePush() 
{
  // First stop extraction 
  stopExtraction();

  if (activeParameter == PARAMETER_VOLUME) {
    parameters.temperature.size = TEXT_SIZE_LARGE;
    parameters.volume.size      = TEXT_SIZE_SMALL;
    parameters.duration.size    = TEXT_SIZE_SMALL;
    activeParameter             = PARAMETER_TEMPERATURE;
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
  previousMillis200 = previousMillis;
  
  if (activeParameter == PARAMETER_VOLUME) {
    EEPROM.update(PARAMETERS_VOLUME_ADDR, parameters.volume.value);
    blinkVolume(parameters.volume);
  } else if (activeParameter == PARAMETER_TEMPERATURE) {
    EEPROM.update(PARAMETERS_TEMPERATURE_ADDR, parameters.temperature.value);
    blinkTemperature(parameters.temperature);
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
  parameters.volume.value      = EEPROM.read(PARAMETERS_VOLUME_ADDR);

  // Display setup
  beginDisplay();

  // Draw the 3 icons and parameters (Temperature, extraction volume setting and extraction time counter)
  drawIcons();
  drawParameters(parameters);

  // Setup rotary button
  encoder.begin();

  // PID setup
  windowStartTime = millis();

  //initialize the variables we're linked to
  Setpoint = (double)parameters.temperature.value;

  //tell the PID to range between 0 and the full window size
  myPID.SetOutputLimits(0, WindowSize);

  //turn the PID on
  myPID.SetMode(AUTOMATIC);

  // Setup MAX31865
  thermo.begin(MAX31865_3WIRE);  // set to 2WIRE or 4WIRE as necessary
}

void loop() 
{
  if (activeParameter == PARAMETER_VOLUME) {
    // Duration counter: one step per second
    unsigned long currentMillis = millis();
    if (counting) {
      if (currentMillis - previousMillis >= 1000) {
        previousMillis += 1000;
        parameters.duration.value++;
        if (parameters.duration.value > parameters.duration.maxValue) {
          parameters.duration.value = 0;
        }
        drawDuration(parameters.duration);   // draw after counting: 1 is shown after one second
      }
      if (currentMillis - previousMillis200 >= 200) {
          previousMillis200 += 200;
          // Read volume
          parameters.volume.value = getVolume(parameters.duration.value);
          drawVolume(parameters.volume);
        }
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

  // PID 
  Input = 80.0;//thermo.temperature(RNOMINAL, RREF);
  myPID.Compute();

  // Turn the output pin on/off based on pid output
  if (millis() - windowStartTime > WindowSize){ 
    //time to shift the Relay Window
    windowStartTime += WindowSize;
  }

  if (Output > millis() - windowStartTime){
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(LED_BUILTIN, HIGH);
    DEBUG_PRINT("Relay on\n");
    DEBUG_PRINT(Output);
    DEBUG_PRINT("\n");
  }else{
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_BUILTIN, LOW); 
    DEBUG_PRINT("Relay OFF\n");
  }
}
