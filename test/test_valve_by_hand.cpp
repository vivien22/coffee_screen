// Config.h with DRIVE_PUMP on and DRIVE_VALVE off (the "pump" flavour of run.sh; the 3-way valve is operated by hand). An extraction
// starts and the pump relay follows its phases, but the valve output never goes on.
#include <vector>
#include <string>
#include <cstring>
#define private public
#include "src/coffee/CoffeeMachine.h"
#undef private
#include "src/Config.h"
#include <cstdio>
#include "EEPROM.h"
#include "settings_test.h"
#include "src/settings/Settings.h"
#include <SPI.h>
#include "TestSupport.h"

void beginDisplay(){} void showDisplay(){} void beginWelcome(){} void endWelcome(){} void showStandbyPicture(){} void drawIcons(){}
void drawTemperature(double,TextSize){} void drawVolume(uint16_t,TextSize,PhaseTag,bool){}
void drawDuration(uint16_t,TextSize,PhaseTag,bool){}
void clearTemperature(){} void clearVolume(){} void clearDuration(){} void drawHeaterIndicator(bool){} void drawPhaseTag(PhaseTag,bool){} void drawDrop(bool,bool){} void drawTemperatureError(){}
void drawSteamIcon(){} void clearScreen(){} void drawSteamTemperature(double,bool){}

// The MAX31865 answers the RTD register (a probe at 93 degrees, PT100, Callendar-Van Dusen)
static uint16_t rtdRegister=0; static int byteIndex=0;
static uint8_t answer(uint8_t v){ if(v==0x01){ byteIndex=0; return 0; } if(v!=0) { byteIndex=-1; return 0; }
  if(byteIndex<0) return 0; return byteIndex++==0 ? (uint8_t)(rtdRegister>>8) : (uint8_t)rtdRegister; }
static void probeAt(double celsius){
  const double A=3.9083e-3,B=-5.775e-7; const double r=RNOMINAL*(1+A*celsius+B*celsius*celsius);
  rtdRegister=(uint16_t)(((uint16_t)(r/RREF*32768.0+0.5))<<1);
}

#define PUMP   (pins[PIN_PUMP_RELAY])
#define VALVE  (pins[PIN_VALVE_RELAY])

int main(){
#ifndef DRIVE_PUMP
  printf("this test is for the Config.h with DRIVE_PUMP\n"); return 1;
#endif
#ifdef DRIVE_VALVE
  printf("this test is for the Config.h without DRIVE_VALVE\n"); return 1;
#endif
  SPI.hook=answer;
  memset(EEPROM.mem,0xFF,64);
  probeAt(93.0);
  CoffeeMachine m; t=1000; m.begin();
  for(int i=0;i<10;i++){ t+=100; m.update(); }
  CHECK(PUMP==0 && VALVE==0);

  // AUTO: a push starts the extraction, the pump goes on, the valve stays off
  m.handleStartButton(); t+=100; m.update();
  CHECK(m.extraction_.isActive());
  CHECK(PUMP==1 && VALVE==0);
  for(int i=0;i<5;i++){ t+=100; m.update(); CHECK(VALVE==0); }
  m.extraction_.stop(EXTRACTION_FINISHED); t+=100; m.update();
  CHECK(PUMP==0 && VALVE==0);

  // MANUAL: same
  m.handleLongPush(); m.handleStartButton(); t+=100; m.update();
  CHECK(m.extraction_.state()==EXTRACTION_MANUAL && PUMP==1 && VALVE==0);
  m.extraction_.stop(EXTRACTION_FINISHED); t+=100; m.update();
  CHECK(PUMP==0 && VALVE==0);
  printf(fails?"%d FAIL\n":"ALL OK\n",fails); return fails!=0;
}
