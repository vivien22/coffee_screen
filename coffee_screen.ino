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

  All positions and colors (icons and texts) are in icons.h.

  Naming conventions (all identifiers in English):
    macros and constants: UPPER_SNAKE_CASE
    functions and variables: camelCase
    types (enum): PascalCase
*/

#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include "fonts.h"
#include "icons.h"
#include "CJMCU111.h"

// ---------- Pins ----------
#define PIN_TFT_CS       10
#define PIN_TFT_DC       7
#define PIN_TFT_RST      8
#define PIN_TFT_BL       9     // backlight
#define PIN_ENCODER_GA   2     // interrupt pins: D2 and D3 on Nano
#define PIN_ENCODER_GB   3

Adafruit_GC9A01A tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

// ---------- CJMCU-111 rotary encoder (adjusts the selected value) ----------
#define ENCODER_STEP     1     // value change per detent
#define VALUE_MIN        0
#define VALUE_MAX        999
#define LONG_PUSH_MS     800   // a push held this long (ms) is a long push (reported before the release)
#define DOUBLE_PUSH_MS   350   // max delay (ms) between two short pushes for a double push (0 = disabled)
                               // a short push is reported once this delay has passed

CJMCU111 encoder(PIN_ENCODER_GA, PIN_ENCODER_GB, LONG_PUSH_MS, DOUBLE_PUSH_MS);

// ---------- Fonts (digits in two sizes, units) ----------
#define FONT_SMALL (&FontSmall)   // digits, small size
#define FONT_LARGE (&FontLarge)   // digits, large size
#define FONT_UNIT  (&FontUnit)    // units: C, ml, s

enum TextSize {
  TEXT_SIZE_SMALL,
  TEXT_SIZE_LARGE
};

// Positions (icons and texts) are in icons.h

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

// Temperature in °C (recommended range: 0 to 99)
void drawTemperature(int value, TextSize size) {
  char text[6];
  itoa(value, text, 10);

  tft.fillRect(TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT, COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  if (size == TEXT_SIZE_SMALL) {
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

// Volume in ml (recommended range: 0 to 999)
void drawVolume(int value, TextSize size) {
  char text[6];
  itoa(value, text, 10);

  tft.fillRect(VOLUME_ZONE_X, VOLUME_ZONE_Y, VOLUME_ZONE_WIDTH, VOLUME_ZONE_HEIGHT, COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  if (size == TEXT_SIZE_SMALL) {
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

// Duration in seconds (recommended range: 0 to 999)
void drawDuration(int value, TextSize size) {
  char text[6];
  itoa(value, text, 10);

  tft.fillRect(DURATION_ZONE_X, DURATION_ZONE_Y, DURATION_ZONE_WIDTH, DURATION_ZONE_HEIGHT, COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  if (size == TEXT_SIZE_SMALL) {
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

// ------------------------------------------------------------
// Value adjusted by the encoder (a short push selects the next one)
enum ActiveParameter {
  PARAMETER_VOLUME,
  PARAMETER_DURATION,
  PARAMETER_TEMPERATURE
};

int volume = 60;                // volume shown at startup
int duration = 0;               // duration shown at startup
int temperature = 90;           // temperature shown at startup

TextSize temperatureSize = TEXT_SIZE_SMALL;
TextSize volumeSize = TEXT_SIZE_LARGE;
TextSize durationSize = TEXT_SIZE_SMALL;
ActiveParameter activeParameter = PARAMETER_VOLUME;

unsigned long previousMillis = 0;
bool counting = false;

// Short push
void onShortPush() {
  if(counting==false){
    counting=true;
    previousMillis = millis();
  }else if(counting==true){
    counting=false;
  }
}

// Double push: two short pushes less than DOUBLE_PUSH_MS apart (detected by the CJMCU111 library)
void onDoublePush() {
  if (activeParameter == PARAMETER_VOLUME) {
    temperatureSize = TEXT_SIZE_SMALL;
    volumeSize = TEXT_SIZE_SMALL;
    durationSize = TEXT_SIZE_LARGE;
    activeParameter = PARAMETER_DURATION;
  } else if (activeParameter == PARAMETER_DURATION) {
    temperatureSize = TEXT_SIZE_LARGE;
    volumeSize = TEXT_SIZE_SMALL;
    durationSize = TEXT_SIZE_SMALL;
    activeParameter = PARAMETER_TEMPERATURE;
  } else if (activeParameter == PARAMETER_TEMPERATURE) {
    temperatureSize = TEXT_SIZE_SMALL;
    volumeSize = TEXT_SIZE_LARGE;
    durationSize = TEXT_SIZE_SMALL;
    activeParameter = PARAMETER_VOLUME;
  }

  drawTemperature(temperature, temperatureSize);
  drawVolume(volume, volumeSize);
  drawDuration(duration, durationSize);
}

// Long push: held for LONG_PUSH_MS or more (reported when the button is released)
void onLongPush() {
  duration = 0;
  drawDuration(duration, durationSize);
}

void setup() {

  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, LOW);

  delay(200);   // let the 3.3V supply and the screen settle at power-on
  tft.begin();
  tft.fillScreen(COLOR_BG);

  digitalWrite(PIN_TFT_BL, HIGH);

  delay(100);

  drawIcons();
  drawTemperature(temperature, temperatureSize);
  drawVolume(volume, volumeSize);
  drawDuration(duration, durationSize);

  encoder.begin();
}
void loop() {
  
  unsigned long currentMillis = millis();
  if (counting) {
    if (currentMillis - previousMillis >= 1000) {
      previousMillis += 1000;
      drawDuration(duration++, durationSize);
      if (duration > 999) {
        duration = 0;
      }
    }
  }

  byte leftSteps, rightSteps;
  PushEvent pushEvent;
  if (encoder.update(leftSteps, rightSteps, pushEvent)) {

    if (activeParameter == PARAMETER_VOLUME) {
      int newValue = constrain(volume + (rightSteps - leftSteps) * ENCODER_STEP, VALUE_MIN, VALUE_MAX);
      if (newValue != volume) {
        volume = newValue;
        drawVolume(volume, volumeSize);
      }
    } else if (activeParameter == PARAMETER_DURATION) {
      int newValue = constrain(duration + (rightSteps - leftSteps) * ENCODER_STEP, VALUE_MIN, VALUE_MAX);
      if (newValue != duration) {
        duration = newValue;
        drawDuration(duration, durationSize);
      }
    } else if (activeParameter == PARAMETER_TEMPERATURE) {
      int newValue = constrain(temperature + (rightSteps - leftSteps) * ENCODER_STEP, VALUE_MIN, VALUE_MAX);
      if (newValue != temperature) {
        temperature = newValue;
        drawTemperature(temperature, temperatureSize);
      }
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
