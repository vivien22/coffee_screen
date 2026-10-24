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

  Positions mesurées sur l'image d'origine (1254 px) et mises à l'échelle 240 px.
  Les positions des icônes sont dans icones.h (ICON_xxx_X / ICON_xxx_Y).
  Pour les textes, Y est la ligne de base (baseline) de la police GFX.
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

// ---------- Température : "92 °C" (aligné à gauche) ----------
#define TEMP_X          98
#define TEMP_BASE       71     // ligne de base du nombre
#define TEMP_UNIT_BASE  68     // ligne de base du "C"
#define TEMP_DEG_Y      53     // centre du petit cercle du degré
#define TEMP_CLR_X      95     // zone effacée à chaque mise à jour
#define TEMP_CLR_Y      39
#define TEMP_CLR_W      83
#define TEMP_CLR_H      34

// ---------- Volume : "38 ml" (aligné à gauche, jusqu'à 3 chiffres) ----------
#define VOL_X           80
#define VOL_BASE        145
#define VOL_UNIT_BASE   144
#define VOL_CLR_X       77     // 77 -> 240 : largeur max à 3 chiffres, sans toucher la goutte
#define VOL_CLR_Y       96
#define VOL_CLR_W       163
#define VOL_CLR_H       52

// ---------- Temps : "48 s" (aligné à gauche, jusqu'à 3 chiffres) ----------
#define TEMPS_X         110
#define TEMPS_BASE      201
#define TEMPS_UNIT_BASE 201
#define TEMPS_CLR_X     107     // 107 -> 205, sans toucher l'horloge
#define TEMPS_CLR_Y     169
#define TEMPS_CLR_W     98
#define TEMPS_CLR_H     34

// ------------------------------------------------------------
void afficherIcones() {
  // drawRGBBitmap lit le tableau en PROGMEM (const uint16_t[])
  tft.drawRGBBitmap(ICON_THERMO_X,  ICON_THERMO_Y,  ICON_THERMO,  ICON_THERMO_W,  ICON_THERMO_H);
  tft.drawRGBBitmap(ICON_GOUTTE_X,  ICON_GOUTTE_Y,  ICON_GOUTTE,  ICON_GOUTTE_W,  ICON_GOUTTE_H);
  tft.drawRGBBitmap(ICON_HORLOGE_X, ICON_HORLOGE_Y, ICON_HORLOGE, ICON_HORLOGE_W, ICON_HORLOGE_H);
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
  tft.drawCircle(x + 6, TEMP_DEG_Y, 4, COLOR_TEXT);   // symbole degré (anneau de 2 px)
  tft.drawCircle(x + 6, TEMP_DEG_Y, 3, COLOR_TEXT);
  tft.setFont(FONT_UNITE);
  tft.setCursor(x + 11, TEMP_UNIT_BASE);
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
  tft.setCursor(x + 5, VOL_UNIT_BASE);
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
  tft.setCursor(x + 1, TEMPS_UNIT_BASE);
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
