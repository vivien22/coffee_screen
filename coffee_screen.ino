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

  Pins, settings, fonts and data types are in coffee_screen.h.
  All positions and colors (icons and texts) are in icons.h.

  Naming conventions (all identifiers in English):
    macros and constants: UPPER_SNAKE_CASE
    functions and variables: camelCase
    types (enum, struct): PascalCase
*/

#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <EEPROM.h>
#include "coffee_screen.h"
#include "CJMCU111.h"

//#define SERIAL_DEBUG

Adafruit_GC9A01A tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
CJMCU111 encoder(PIN_ENCODER_GA, PIN_ENCODER_GB, LONG_PUSH_MS, DOUBLE_PUSH_MS);

// Parameters shown at startup (value, size of the digits, maximum value)
ScreenParameters parameters = {
  { 90, TEXT_SIZE_SMALL, TEMPERATURE_MAX },  // temperature (°C)
  { 60, TEXT_SIZE_LARGE, VOLUME_MAX },       // volume (ml)
  { 0, TEXT_SIZE_SMALL, DURATION_MAX }       // duration (s)
};
ActiveParameter activeParameter = PARAMETER_VOLUME;

unsigned long previousMillis = 0;
bool counting = false;

// ------------------------------------------------------------
// Blend COLOR_ICON with COLOR_BG; opacity: 0 (background) to 15 (full color)
uint16_t blendColor(uint8_t opacity) {
  uint8_t r = (COLOR_BG >> 11) & 0x1F, g = (COLOR_BG >> 5) & 0x3F, b = COLOR_BG & 0x1F;
  int8_t dr = ((COLOR_ICON >> 11) & 0x1F) - r;
  int8_t dg = ((COLOR_ICON >> 5) & 0x3F) - g;
  int8_t db = (COLOR_ICON & 0x1F) - b;
  return ((uint16_t)(r + dr * opacity / 15) << 11) | ((uint16_t)(g + dg * opacity / 15) << 5) | (uint16_t)(b + db * opacity / 15);
}

// Draw an icon stored as 4-bit opacity (PROGMEM); background pixels are not redrawn
void drawIcon(int16_t x, int16_t y, const uint8_t *opacities, uint8_t width, uint8_t height) {
  tft.startWrite();
  for (uint16_t i = 0; i < (uint16_t)width * height; i++) {
    uint8_t opacity = pgm_read_byte(&opacities[i >> 1]);
    opacity = (i & 1) ? (opacity & 0x0F) : (opacity >> 4);
    if (opacity) tft.writePixel(x + i % width, y + i / width, blendColor(opacity));
  }
  tft.endWrite();
}

void drawIcons() {
  drawIcon(ICON_THERMOMETER_X, ICON_THERMOMETER_Y, ICON_THERMOMETER, ICON_THERMOMETER_WIDTH, ICON_THERMOMETER_HEIGHT);
  drawIcon(ICON_DROP_X,        ICON_DROP_Y,        ICON_DROP,        ICON_DROP_WIDTH,        ICON_DROP_HEIGHT);
  drawIcon(ICON_CLOCK_X,       ICON_CLOCK_Y,       ICON_CLOCK,       ICON_CLOCK_WIDTH,       ICON_CLOCK_HEIGHT);
}

void drawTemperature(const Parameter &parameter) {
  char text[6];
  itoa(parameter.value, text, 10);

  tft.fillRect(TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT, COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  if (parameter.size == TEXT_SIZE_SMALL) {
    tft.setFont(FONT_SMALL);
  } else {
    tft.setFont(FONT_LARGE);
  }
  tft.setCursor(TEMPERATURE_X, TEMPERATURE_BASELINE);
  tft.print(text);

  int16_t xNumberEnd = tft.getCursorX();
  tft.drawCircle(xNumberEnd + TEMPERATURE_DEGREE_DX, TEMPERATURE_DEGREE_Y, TEMPERATURE_DEGREE_RADIUS, COLOR_TEXT);       // degree sign (2 px ring)
  tft.drawCircle(xNumberEnd + TEMPERATURE_DEGREE_DX, TEMPERATURE_DEGREE_Y, TEMPERATURE_DEGREE_RADIUS - 1, COLOR_TEXT);
  tft.setFont(FONT_UNIT);
  tft.setCursor(xNumberEnd + TEMPERATURE_UNIT_DX, TEMPERATURE_UNIT_BASELINE);
  tft.print('C');
}

void drawVolume(const Parameter &parameter) {
  char text[6];
  itoa(parameter.value, text, 10);

  tft.fillRect(VOLUME_ZONE_X, VOLUME_ZONE_Y, VOLUME_ZONE_WIDTH, VOLUME_ZONE_HEIGHT, COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  if (parameter.size == TEXT_SIZE_SMALL) {
    tft.setFont(FONT_SMALL);
  } else {
    tft.setFont(FONT_LARGE);
  }
  tft.setCursor(VOLUME_X, VOLUME_BASELINE);
  tft.print(text);

  int16_t xNumberEnd = tft.getCursorX();
  tft.setFont(FONT_UNIT);
  tft.setCursor(xNumberEnd + VOLUME_UNIT_DX, VOLUME_UNIT_BASELINE);
  tft.print(F("ml"));
}

void drawDuration(const Parameter &parameter) {
  char text[6];
  itoa(parameter.value, text, 10);

  tft.fillRect(DURATION_ZONE_X, DURATION_ZONE_Y, DURATION_ZONE_WIDTH, DURATION_ZONE_HEIGHT, COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  if (parameter.size == TEXT_SIZE_SMALL) {
    tft.setFont(FONT_SMALL);
  } else {
    tft.setFont(FONT_LARGE);
  }
  tft.setCursor(DURATION_X, DURATION_BASELINE);
  tft.print(text);

  int16_t xNumberEnd = tft.getCursorX();
  tft.setFont(FONT_UNIT);
  tft.setCursor(xNumberEnd + DURATION_UNIT_DX, DURATION_UNIT_BASELINE);
  tft.print('s');
}

void drawParameters() {
  drawTemperature(parameters.temperature);
  drawVolume(parameters.volume);
  drawDuration(parameters.duration);
}

// ------------------------------------------------------------
// Short push: start / stop the duration counter.
// A short push is reported after the double push delay, so the counter is timed from
// the moment the button was pressed (encoder.eventPressTime()), not from now.
void onShortPush() {

#ifdef SERIAL_DEBUG
  Serial.print("onShortPush\n");
#endif

  if (counting == false) {
    counting = true;
    previousMillis = encoder.eventPressTime();   // time of the last counted second
#ifdef SERIAL_DEBUG
  Serial.print("Start\n");
#endif
  } else {
    counting = false;

    // give back the seconds counted after the press (at most 2: push + delay < 2 s)
    unsigned long pressMs = encoder.eventPressTime();
    int undoneSeconds = 0;
    while ((long)(previousMillis - pressMs) > 0 && undoneSeconds < 2) {
      previousMillis -= 1000;
      undoneSeconds++;
    }

    // Reset to default value
    parameters.duration.value = EEPROM.read(PARAMETERS_DURATION_ADDR);
    drawDuration(parameters.duration);

#ifdef SERIAL_DEBUG
  Serial.print("Stop\n");
#endif
  }
}

// Double push: select the next parameter (it gets the large digits)
void onDoublePush() {
  TextSize previousTemperatureSize = parameters.temperature.size;

  if (activeParameter == PARAMETER_VOLUME) {
    parameters.temperature.size = TEXT_SIZE_SMALL;
    parameters.volume.size = TEXT_SIZE_SMALL;
    parameters.duration.size = TEXT_SIZE_LARGE;
    activeParameter = PARAMETER_DURATION;
  } else if (activeParameter == PARAMETER_DURATION) {
    parameters.temperature.size = TEXT_SIZE_LARGE;
    parameters.volume.size = TEXT_SIZE_SMALL;
    parameters.duration.size = TEXT_SIZE_SMALL;
    activeParameter = PARAMETER_TEMPERATURE;
  } else if (activeParameter == PARAMETER_TEMPERATURE) {
    parameters.temperature.size = TEXT_SIZE_SMALL;
    parameters.volume.size = TEXT_SIZE_LARGE;
    parameters.duration.size = TEXT_SIZE_SMALL;
    activeParameter = PARAMETER_VOLUME;
  }
  drawParameters();
}

// Long push: reported as soon as the button has been held for LONG_PUSH_MS (before the release)
void onLongPush() {
  previousMillis = millis();   // while counting, the next second starts from the reset
  
  if (activeParameter == PARAMETER_VOLUME) {
    EEPROM.update(PARAMETERS_VOLUME_ADDR, parameters.volume.value);
  } else if (activeParameter == PARAMETER_DURATION) {
    EEPROM.update(PARAMETERS_DURATION_ADDR, parameters.duration.value);
  } else if (activeParameter == PARAMETER_TEMPERATURE) {
    EEPROM.update(PARAMETERS_TEMPERATURE_ADDR, parameters.temperature.value);
  }
}

void setup() {

#ifdef SERIAL_DEBUG
  Serial.begin(115200);
#endif

  parameters.temperature.value = EEPROM.read(PARAMETERS_TEMPERATURE_ADDR);
  parameters.volume.value = EEPROM.read(PARAMETERS_VOLUME_ADDR);
  parameters.duration.value = EEPROM.read(PARAMETERS_DURATION_ADDR);

  // Set Blk Pin of the screen to Low in order to avoid artefacts before the screen is filled of black 
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, LOW);

  tft.begin();
  tft.fillScreen(COLOR_BG);

  // Set Blk Pin to High
  digitalWrite(PIN_TFT_BL, HIGH);

  drawIcons();
  drawParameters();

  encoder.begin();
}

void loop() {

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

  byte leftSteps, rightSteps;
  PushEvent pushEvent;
  if (encoder.update(leftSteps, rightSteps, pushEvent)) {

    int delta = (rightSteps - leftSteps) * ENCODER_STEP;
    if (activeParameter == PARAMETER_VOLUME) {
      if (parameters.volume.adjust(delta)) drawVolume(parameters.volume);
    } else if (activeParameter == PARAMETER_DURATION) {
      if (parameters.duration.adjust(delta)) drawDuration(parameters.duration);
    } else if (activeParameter == PARAMETER_TEMPERATURE) {
      if (parameters.temperature.adjust(delta)) drawTemperature(parameters.temperature);
    }

    if (pushEvent == PUSH_SHORT) {
      onShortPush();
    } else if (pushEvent == PUSH_DOUBLE) {
      onDoublePush();
    } else if (pushEvent == PUSH_LONG) {
      onLongPush();
    }
  }
}
