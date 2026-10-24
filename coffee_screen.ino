/*
  Afficheur café GC9A01 : icônes + valeurs (température, volume, temps)
  Arduino Nano + Adafruit_GC9A01A
  (nécessite aussi Adafruit GFX Library et Adafruit BusIO)

  Câblage (SPI matériel du Nano) :
    GC9A01   Nano
    VCC   -> 3.3V
    GND   -> GND
    SCL   -> D13 (SCK)
    SDA   -> D11 (MOSI)
    CS    -> D10
    DC    -> D7
    RES   -> D8

  Attention : le Nano sort du 5V alors que le GC9A01 est en 3.3V.
  Utilise un adaptateur de niveau ou des résistances en série (1k) sur SCL, SDA, CS, DC, RES.

  Toutes les positions (icônes et textes) sont dans icones.h.
*/

#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include "PolicesCafe.h"
#include "icones.h"
#include "CJMCU111.h"

#define PIN_CS  10
#define PIN_DC  7
#define PIN_RST 8

Adafruit_GC9A01A tft(PIN_CS, PIN_DC, PIN_RST);

// ---------- Encodeur rotatif CJMCU-111 (règle le volume) ----------
#define ENC_PIN_GA  2          // broches d'interruption : D2 et D3 sur Nano
#define ENC_PIN_GB  3
#define VOL_PAS     1          // ml par cran
#define VOL_MIN     0
#define VOL_MAX     999

CJMCU111 encodeur(ENC_PIN_GA, ENC_PIN_GB);

// ---------- Couleur et polices ----------
#define COLOR_TEXT 0xFFBC                   // crème (254,245,228)
#define FONT_VALEUR (&FontValeur)   // 92 et 48 (chiffres uniquement)
#define FONT_GRANDE (&FontGrande)   // 38 (chiffres uniquement)
#define FONT_UNITE  (&FontUnite)    // C, m, l, s

// Les positions (icônes et textes) sont dans icones.h

// ------------------------------------------------------------
// Mélange COLOR_ICON avec COLOR_BG ; alpha : 0 (fond) à 15 (couleur pleine)
uint16_t melange(uint8_t alpha) {
  uint8_t r = (COLOR_BG >> 11) & 0x1F, g = (COLOR_BG >> 5) & 0x3F, b = COLOR_BG & 0x1F;
  int8_t dr = ((COLOR_ICON >> 11) & 0x1F) - r;
  int8_t dg = ((COLOR_ICON >> 5) & 0x3F) - g;
  int8_t db = (COLOR_ICON & 0x1F) - b;
  return ((uint16_t)(r + dr * alpha / 15) << 11) | ((uint16_t)(g + dg * alpha / 15) << 5) | (uint16_t)(b + db * alpha / 15);
}

// Dessine une icône stockée en alpha 4 bits (PROGMEM), les pixels de fond ne sont pas redessinés
void dessinerIcone(int16_t x, int16_t y, const uint8_t *alpha, uint8_t w, uint8_t h) {
  tft.startWrite();
  for (uint16_t i = 0; i < (uint16_t)w * h; i++) {
    uint8_t a = pgm_read_byte(&alpha[i >> 1]);
    a = (i & 1) ? (a & 0x0F) : (a >> 4);
    if (a) tft.writePixel(x + i % w, y + i / w, melange(a));
  }
  tft.endWrite();
}

void afficherIcones() {
  dessinerIcone(ICON_THERMO_X,  ICON_THERMO_Y,  ICON_THERMO,  ICON_THERMO_W,  ICON_THERMO_H);
  dessinerIcone(ICON_GOUTTE_X,  ICON_GOUTTE_Y,  ICON_GOUTTE,  ICON_GOUTTE_W,  ICON_GOUTTE_H);
  dessinerIcone(ICON_HORLOGE_X, ICON_HORLOGE_Y, ICON_HORLOGE, ICON_HORLOGE_W, ICON_HORLOGE_H);
}

// Température en °C (plage conseillée : 0 à 99)
void afficherTemperature(int t) {
  char buf[6];
  itoa(t, buf, 10);

  tft.fillRect(TEMP_CLR_X, TEMP_CLR_Y, TEMP_CLR_W, TEMP_CLR_H, COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  tft.setFont(FONT_VALEUR);
  tft.setCursor(TEMP_X, TEMP_BASE);
  tft.print(buf);

  int16_t x = tft.getCursorX();
  tft.drawCircle(x + TEMP_DEG_DX, TEMP_DEG_Y, TEMP_DEG_R, COLOR_TEXT);       // symbole degré (anneau de 2 px)
  tft.drawCircle(x + TEMP_DEG_DX, TEMP_DEG_Y, TEMP_DEG_R - 1, COLOR_TEXT);
  tft.setFont(FONT_UNITE);
  tft.setCursor(x + TEMP_UNIT_DX, TEMP_UNIT_BASE);
  tft.print('C');
}

// Volume en ml (plage conseillée : 0 à 999)
void afficherVolume(int ml) {
  char buf[6];
  itoa(ml, buf, 10);

  tft.fillRect(VOL_CLR_X, VOL_CLR_Y, VOL_CLR_W, VOL_CLR_H, COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  tft.setFont(FONT_GRANDE);
  tft.setCursor(VOL_X, VOL_BASE);
  tft.print(buf);

  int16_t x = tft.getCursorX();
  tft.setFont(FONT_UNITE);
  tft.setCursor(x + VOL_UNIT_DX, VOL_UNIT_BASE);
  tft.print(F("ml"));
}

// Temps en secondes (plage conseillée : 0 à 999)
void afficherTemps(int s) {
  char buf[6];
  itoa(s, buf, 10);

  tft.fillRect(TEMPS_CLR_X, TEMPS_CLR_Y, TEMPS_CLR_W, TEMPS_CLR_H, COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  tft.setFont(FONT_VALEUR);
  tft.setCursor(TEMPS_X, TEMPS_BASE);
  tft.print(buf);

  int16_t x = tft.getCursorX();
  tft.setFont(FONT_UNITE);
  tft.setCursor(x + TEMPS_UNIT_DX, TEMPS_UNIT_BASE);
  tft.print('s');
}

// ------------------------------------------------------------
int volume = 60;                // volume affiché au démarrage
int tps = 0;                    // volume affiché au démarrage

void setup() {
  delay(500);   // laisse l'alimentation 3.3V et l'écran se stabiliser à la mise sous tension
  tft.begin();
  tft.fillScreen(COLOR_BG);

  afficherIcones();
  afficherTemperature(92);
  afficherVolume(volume);
  afficherTemps(tps);
 
  encodeur.begin();
}

unsigned long tempsPrecedent = 0;
bool compteur = false;

void loop() {
  unsigned long tempsActuel = millis();
  if(compteur){
    if (tempsActuel - tempsPrecedent >= 1000) {
      tempsPrecedent += 1000;
      afficherTemps(tps++);
      if(tps>999){
        tps=0;
      }
    }
  }

  byte gauche, droite, push;
  if (encodeur.read(gauche, droite, push)) {
    int nouveau = constrain(volume + (droite - gauche) * VOL_PAS, VOL_MIN, VOL_MAX);
    if (nouveau != volume) {
      volume = nouveau;
      afficherVolume(volume);
    }
    if(push){
      compteur=!compteur;
      tps=0;
      tempsPrecedent=tempsActuel-1000;
    }
  }
}
