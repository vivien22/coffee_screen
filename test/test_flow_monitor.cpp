// The diagnostic build (FLOW_MONITOR, the "flow monitor" of run.sh: built with -DFLOW_MONITOR): the boiler is off (no heating, no
// PID), the volume field shows the water counted by In and the temperature field the water counted by Out; everything else works
// as in the machine (an extraction starts, the pump and the valve go on).
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

#ifndef FLOW_MONITOR
#error "test_flow_monitor needs -DFLOW_MONITOR"
#endif

std::vector<std::string> calls;
static void L(const char*f,double a=0,int b=0,int c=0,int d=0){char s[80];snprintf(s,80,"%s(%g,%d,%d,%d)",f,a,b,c,d);calls.push_back(s);}
void beginDisplay(){} void showDisplay(){} void beginWelcome(){} void endWelcome(){} void showStandbyPicture(){} void drawIcons(){}
void drawTemperature(double v,TextSize s){L("temp",v,s);} void drawVolume(uint16_t v,TextSize s,PhaseTag g,bool r){L("vol",v,s,g,r);}
void drawDuration(uint16_t v,TextSize s,PhaseTag g,bool r){L("dur",v,s,g,r);}
void clearTemperature(){} void clearVolume(){} void clearDuration(){} void clearValues(){} void drawHeaterIndicator(bool){} void drawPhaseTag(PhaseTag,bool){} void drawDrop(bool,bool){} void drawTemperatureError(){L("temperr");}

// The MAX31865 answers the RTD register: the test sets the temperature the probe "measures" (PT100, Callendar-Van Dusen)
static uint16_t rtdRegister=0; static int byteIndex=0;
static uint8_t answer(uint8_t v){ if(v==0x01){ byteIndex=0; return 0; } if(v!=0) { byteIndex=-1; return 0; }
  if(byteIndex<0) return 0; return byteIndex++==0 ? (uint8_t)(rtdRegister>>8) : (uint8_t)rtdRegister; }
static void probeAt(double celsius){
  const double A=3.9083e-3,B=-5.775e-7; const double r=RNOMINAL*(1+A*celsius+B*celsius*celsius);
  rtdRegister=(uint16_t)(((uint16_t)(r/RREF*32768.0+0.5))<<1);
}

static bool has(const char *prefix){ for(auto&c:calls) if(c.rfind(prefix,0)==0) return true; return false; }
static void run(CoffeeMachine &m,int passes){ for(int i=0;i<passes;i++){ t+=100; m.update(); } }

#define HEATER (pins[PIN_HEATER_RELAY])
#define PUMP   (pins[PIN_PUMP_RELAY])
#define VALVE  (pins[PIN_VALVE_RELAY])

int main(){
  SPI.hook=answer;
  memset(EEPROM.mem,0xFF,64);
  probeAt(30.0);                                           // a cold boiler: the normal machine would heat at full power
  CoffeeMachine m; t=1000; m.begin();

  // ---- no heating, no PID: the heater output stays low, whatever the probe says
  for(int i=0;i<200;i++){ t+=100; m.update(); CHECK(HEATER==0); }
  CHECK(!m.heater_.isOn());
  probeAt(120.0); run(m,30); CHECK(HEATER==0);
  probeAt(30.0); run(m,30); CHECK(HEATER==0);

  // ---- the volume field shows In, the temperature field shows Out (millilitres, since the start of the extraction)
  m.flow_.in=12.4f; m.flow_.out=3.2f; calls.clear(); run(m,20);
  CHECK(has("vol(12,"));                                    // 12.4 ml of In
  CHECK(has("temp(3.2,"));                                  // 3.2 ml of Out
  CHECK(!has("temp(25,") && !has("temp(30,"));              // not the probe (30 C, 25 C at the grouphead)
  CHECK(!has("temperr"));

  // ---- rounded to the millilitre, three digits for the volume, one decimal for Out under 100
  m.flow_.in=104.6f; m.flow_.out=98.76f; calls.clear(); run(m,20);
  CHECK(has("vol(105,"));
  CHECK(has("temp(98.76,"));
  m.flow_.in=0; m.flow_.out=0; calls.clear(); run(m,20);
  CHECK(has("vol(0,"));

  // ---- the rest is the machine: a push starts an extraction, the pump and the valve go on, the heater stays off
  CHECK(PUMP==0 && VALVE==0);
  m.handleStartButton(); run(m,5);
  CHECK(m.extraction_.isActive() && PUMP==1 && VALVE==1);
  CHECK(HEATER==0);
  m.flow_.in=20.0f; m.flow_.out=7.7f; calls.clear(); run(m,20);
  CHECK(has("vol(20,"));
  CHECK(has("temp(7.7,"));
  CHECK(!has("vol(0,"));                                    // the phase values of the extraction (PRE ...) do not replace the flows
  m.extraction_.stop(EXTRACTION_FINISHED); run(m,5);
  CHECK(PUMP==0 && VALVE==0 && HEATER==0);
  calls.clear(); run(m,20);
  CHECK(!has("vol(0,"));                                    // the flows stay on the screen after the end

  printf(fails?"%d FAIL\n":"ALL OK\n",fails); return fails!=0;
}
