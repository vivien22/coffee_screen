// Without DRIVE_PUMP and DRIVE_VALVE (the "boiler only" flavour of run.sh, a copy of Config.h with the options switched off): the heater is
// controlled as usual, the pump and the valve relays never go on, and no extraction can be started.
#include <algorithm>
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

std::vector<std::string> calls;
static void L(const char*f,double a=0,int b=0,int c=0,int d=0){char s[80];snprintf(s,80,"%s(%g,%d,%d,%d)",f,a,b,c,d);calls.push_back(s);}
void beginDisplay(){} void showDisplay(){} void beginWelcome(){} void endWelcome(){} void showStandbyPicture(){} void drawIcons(){}
void drawTemperature(double v,TextSize s){L("temp",v,s);} void drawVolume(uint16_t v,TextSize s,PhaseTag g,bool r){L("vol",v,s,g,r);}
void drawDuration(uint16_t v,TextSize s,PhaseTag g,bool r){L("dur",v,s,g,r);}
void clearTemperature(){} void clearVolume(){} void clearDuration(){} void drawHeaterIndicator(bool){} void drawPhaseTag(PhaseTag,bool){} void drawDrop(bool,bool){} void drawTemperatureError(){}
void drawSteamIcon(){} void clearScreen(){} void drawSteamTemperature(double,bool){}

// The MAX31865 answers the RTD register: the test sets the temperature the probe "measures" (PT100, Callendar-Van Dusen)
static uint16_t rtdRegister=0; static int byteIndex=0;
static uint8_t answer(uint8_t v){ if(v==0x01){ byteIndex=0; return 0; } if(v!=0) { byteIndex=-1; return 0; }
  if(byteIndex<0) return 0; return byteIndex++==0 ? (uint8_t)(rtdRegister>>8) : (uint8_t)rtdRegister; }
static void probeAt(double celsius){
  const double A=3.9083e-3,B=-5.775e-7; const double r=RNOMINAL*(1+A*celsius+B*celsius*celsius);
  rtdRegister=(uint16_t)(((uint16_t)(r/RREF*32768.0+0.5))<<1);
}

#define HEATER (pins[5])
#define PUMP   (pins[PIN_PUMP_RELAY])
#define VALVE  (pins[PIN_VALVE_RELAY])

int main(){
#if defined(DRIVE_PUMP) || defined(DRIVE_VALVE)
  printf("this test is for the Config.h without DRIVE_PUMP\n"); return 1;
#endif
  SPI.hook=answer;
  memset(EEPROM.mem,0xFF,64);
  probeAt(30.0);                                             // a cold boiler: the heater has to work, at full power
  CoffeeMachine m; t=1000; m.begin();
  CHECK(PUMP==0 && VALVE==0);                                // both relays off from the start
  bool heaterOn=false;
  for(int i=0;i<40;i++){ t+=100; m.update(); heaterOn=heaterOn||HEATER==1; }
  CHECK(heaterOn);                                           // the boiler is controlled
  // pushes: the volume is selected, the start button would start an AUTO extraction
  calls.clear();
  m.handleStartButton(); t+=100; m.update();
  CHECK(!m.extraction_.isActive() && m.extraction_.state()==EXTRACTION_IDLE);
  CHECK(PUMP==0 && VALVE==0);
  bool volumeReset=false; for(auto&c:calls) if(c.rfind("vol(0,",0)==0) volumeReset=true;
  CHECK(!volumeReset);                                       // the screen was not reset as for a start
  // and in MANUAL mode too
  m.handleLongPush(); m.handleStartButton(); t+=100; m.update();
  CHECK(!m.extraction_.isActive() && PUMP==0 && VALVE==0);
  // the relays have their own protection: even a start that does not go through the push handler leaves them off
  m.extraction_.start(t,10,5,5);
  CHECK(m.extraction_.isActive() && PUMP==0 && VALVE==0);
  m.extraction_.stop(EXTRACTION_FINISHED);
  m.extraction_.startManual(t,0);
  CHECK(m.extraction_.isActive() && PUMP==0 && VALVE==0);
  m.extraction_.stop(EXTRACTION_FINISHED);
  // the heater still works afterwards
  heaterOn=false; for(int i=0;i<40;i++){ t+=100; m.update(); heaterOn=heaterOn||HEATER==1; }
  CHECK(heaterOn);
  // a hot boiler: the heater goes off, still no pump nor valve
  probeAt(120.0); for(int i=0;i<300;i++){ t+=100; m.update(); }
  CHECK(HEATER==0 && PUMP==0 && VALVE==0);
  printf(fails?"%d FAIL\n":"ALL OK\n",fails); return fails!=0;
}
