#include <vector>
#include <string>
#include <cstring>
#define private public
#include "src/machine/coffee_machine.h"
#undef private
#include "src/config.h"
#include <cstdio>
#include "EEPROM.h"
#include <SPI.h>
SPIClass SPI;
static unsigned long t=0; unsigned long millis(){return t;} void delay(unsigned long){}
static int pins[32]; void pinMode(uint8_t,uint8_t){} void digitalWrite(uint8_t p,uint8_t v){pins[p]=v;}
int digitalRead(uint8_t){return 0;} void noInterrupts(){} void interrupts(){} SerialT Serial; EEPROMClass EEPROM;
std::vector<std::string> calls;
static void L(const char*f,double a=0,int b=0,int c=0,int d=0){char s[80];snprintf(s,80,"%s(%g,%d,%d,%d)",f,a,b,c,d);calls.push_back(s);}
void beginDisplay(){} void drawIcons(){}
void drawTemperature(double v,TextSize s){L("temp",v,s);} void drawVolume(uint16_t v,TextSize s,PhaseTag g,bool r){L("vol",v,s,g,r);}
void drawDuration(uint16_t v,TextSize s,PhaseTag g,bool r){L("dur",v,s,g,r);}
void clearTemperature(){} void clearVolume(){} void clearDuration(){} void drawHeaterIndicator(bool){} void drawPhaseTag(PhaseTag g,bool v){L("tag",0,g,v);} void drawDrop(bool v,bool m){L("drop",m,v);} void drawTemperatureError(){L("temperr");}

static int fails=0;
#define CHECK(c) do{ if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;} }while(0)
#define PUMPON (pins[4]==1)
static bool has(const char*s){ for(auto&c:calls) if(c==s) return true; return false; }
static void step(CoffeeMachine&m,unsigned long dt){ t+=dt; m.update(); }
static void scenario(int flush){
  memset(EEPROM.mem,0xFF,64); SettingsStore st; st.load(); st.savePreinfusion(0); st.savePreinfusionWait(0); st.saveFlush(flush);
  CoffeeMachine m; t=1000; m.begin(); calls.clear();
  m.handleShortPush();                          // start the extraction (volume selected)
  CHECK(m.extraction_.state()==EXTRACTION_RUNNING);
  { ActiveParameter p=m.screen_.activeParameter(); m.handleDoublePush();   // ignored during the extraction
    CHECK(m.extraction_.state()==EXTRACTION_RUNNING && m.screen_.activeParameter()==p && PUMPON); }
  for(int i=0;i<200;i++){ t+=100; m.flow_.vol=(t-1000)/1000.0f*10; m.update(); if(m.extraction_.state()!=EXTRACTION_RUNNING) break; }
  unsigned long tEnd=t; (void)tEnd;
  if(flush>0){
    CHECK(m.extraction_.state()==EXTRACTION_FLUSH_WAIT);
    // waiting: the extraction values stay, nothing like "target volume" appears even after a long time
    calls.clear(); for(int i=0;i<150;i++){ t+=100; m.update(); }
    CHECK(!has("dur(0,0,0,1)"));
    m.handleShortPush(); CHECK(m.extraction_.state()==EXTRACTION_FLUSHING);
    { ActiveParameter p=m.screen_.activeParameter(); m.handleDoublePush();      // flush: a double push skips it
      CHECK(!m.extraction_.isActive() && m.screen_.activeParameter()!=p); CHECK(pins[4]==0 && pins[16]==0); }
    m.screen_.selectNextParameter(); m.screen_.selectNextParameter(); m.screen_.selectNextParameter(); m.screen_.selectNextParameter();   // back to volume
    CHECK(m.screen_.activeParameter()==PARAMETER_VOLUME);
    m.handleShortPush(); for(int i=0;i<100;i++){ t+=100; m.flow_.vol=(t-1000)/1000.0f*100; m.update(); if(m.extraction_.state()==EXTRACTION_FLUSH_WAIT) break; }
    m.handleShortPush(); CHECK(m.extraction_.state()==EXTRACTION_FLUSHING);
    calls.clear(); unsigned long tf=t;
    for(int i=0;i<100;i++){ t+=100; m.update(); if(!m.extraction_.isActive()) break; }
    CHECK(m.extraction_.state()==EXTRACTION_FINISHED);
    // the target volume (40, large because the volume is selected) and 0 s are shown at once, no hold
    CHECK(has("vol(40,1,0,1)")); CHECK(has("dur(0,0,0,1)"));
    printf("flush=%d: values reset %lu ms after the end of the flush\n",flush,0UL);
  } else {
    CHECK(m.extraction_.state()==EXTRACTION_FINISHED);
    // final values shown, held 10 s
    bool final=false; for(auto&c:calls) if(c.rfind("dur(",0)==0 && c!="dur(0,0,0,1)") final=true; CHECK(final);
    calls.clear(); unsigned long t0=t;
    while(t<t0+9900){ t+=100; m.update(); }
    CHECK(!has("dur(0,0,0,1)")); CHECK(!has("vol(40,1,0,1)"));     // still holding at 9.9 s
    while(t<t0+10200){ t+=100; m.update(); }
    CHECK(has("dur(0,0,0,1)")); CHECK(has("vol(40,1,0,1)"));       // reset after 10 s
    // a new extraction during the hold cancels the reset
    m.handleShortPush(); t+=100; m.update(); calls.clear();
    for(int i=0;i<30;i++){ t+=100; m.flow_.vol=1; m.update(); }
    CHECK(m.extraction_.state()==EXTRACTION_RUNNING);
    m.handleShortPush();   // stop it
    t0=t; calls.clear(); while(t<t0+4000){ t+=100; m.update(); }
    CHECK(!has("dur(0,0,0,1)"));
    m.handleShortPush(); t+=100; m.update();     // start again: cancels the pending hold
    printf("flush=%d: hold 10 s ok\n",flush);
  }
}
// No-water error: the drop blinks every DROP_BLINK_MS until a long push; short / double pushes do nothing
static void noWater(){
  memset(EEPROM.mem,0xFF,64); SettingsStore st; st.load(); st.savePreinfusion(0); st.savePreinfusionWait(0); st.saveFlush(0);
  CoffeeMachine m; t=1000; m.begin(); calls.clear();
  m.handleShortPush(); CHECK(m.extraction_.state()==EXTRACTION_RUNNING);
  calls.clear();
  unsigned long tError=0, tHide=0, tShow=0, tHide2=0;
  for(int i=0;i<(int)(NO_FLOW_TIMEOUT_MS/100)+40;i++){                              // no water at all
    t+=100; calls.clear(); m.update();
    if(!tError && m.extraction_.state()==EXTRACTION_ERROR_NO_WATER) tError=t;
    if(tError && !tHide && has("drop(0,0,0,0)")) tHide=t;                           // hidden at once
    else if(tHide && !tShow && has("drop(0,1,0,0)")) tShow=t;
    else if(tShow && !tHide2 && has("drop(0,0,0,0)")) tHide2=t;
  }
  CHECK(tError>0 && !PUMPON);
  CHECK(tHide==tError);                                                              // hidden when the error is detected
  CHECK(tShow-tHide==DROP_BLINK_MS && tHide2-tShow==DROP_BLINK_MS);                  // toggled every 600 ms
  // short and double pushes are ignored
  m.handleShortPush(); m.handleDoublePush(); t+=100; m.update();
  CHECK(m.extraction_.state()==EXTRACTION_ERROR_NO_WATER && !PUMPON);
  // a long push clears the error: the drop is shown and stops blinking, nothing is started
  m.handleLongPush(); calls.clear(); t+=100; m.update();
  CHECK(m.extraction_.state()==EXTRACTION_IDLE && !m.extraction_.hasNoWaterError());
  bool hidden=false; calls.clear(); for(int i=0;i<30;i++){ t+=100; m.update(); } CHECK(!has("drop(0,0,0,0)"));
  (void)hidden;
  m.handleShortPush(); CHECK(m.extraction_.state()==EXTRACTION_RUNNING);              // a new extraction is possible
  printf("no-water: ok\n");
}
// Temperature sensor fault: "ERR" replaces the temperature, which comes back with the sensor
static void temperatureError(){
  ScreenController sc; Settings in={93,40,10,5,5}; sc.begin(in); sc.showLiveTemperature(0,91.5); calls.clear(); sc.update(0);
  CHECK(!has("temperr(0,0,0,0)"));
  sc.showTemperatureError(); calls.clear(); sc.update(1); CHECK(has("temperr(0,0,0,0)"));
  sc.showTemperatureError(); calls.clear(); sc.update(2); CHECK(calls.empty());             // drawn once
  // the setpoint is still shown while it is edited
  for(int i=0;i<4;i++) sc.selectNextParameter(); CHECK(sc.activeParameter()==PARAMETER_TEMPERATURE);
  calls.clear(); sc.update(3); CHECK(!has("temperr(0,0,0,0)"));
  sc.selectNextParameter(); calls.clear(); sc.update(4); CHECK(has("temperr(0,0,0,0)"));     // back to the live value: still ERR
  sc.showLiveTemperature(5000,90.0); calls.clear(); sc.update(5000); CHECK(!has("temperr(0,0,0,0)") && !calls.empty());
  printf("temperature error: ok\n");
}
// MANUAL mode: a very long push switches the mode (saved), a push starts the pump, the next one stops it
static void manualMode(){
  memset(EEPROM.mem,0xFF,64); SettingsStore st; st.load(); st.savePreinfusion(0); st.saveFlush(0);
  CoffeeMachine m; t=1000; m.begin(); calls.clear();
  CHECK(!m.screen_.settings().manual);
  m.handleVeryLongPush();                               // AUTO -> MANUAL
  CHECK(m.screen_.settings().manual && EEPROM.mem[8]==1 && has("drop(1,1,0,0)"));   // letter M, drop shown
  { CoffeeMachine m2; m2.begin(); CHECK(m2.screen_.settings().manual); }            // remembered after a restart
  m.handleLongPush();                                    // a long push is still a save, not a mode change
  m.handleShortPush();                                   // start the pump
  CHECK(m.extraction_.state()==EXTRACTION_MANUAL && PUMPON && pins[16]==1);
  m.handleVeryLongPush(); CHECK(m.screen_.settings().manual && m.extraction_.state()==EXTRACTION_MANUAL);   // ignored while running
  m.handleDoublePush(); CHECK(m.extraction_.state()==EXTRACTION_MANUAL);
  m.handleLongPush(); CHECK(m.extraction_.state()==EXTRACTION_MANUAL);
  calls.clear();
  for(int i=0;i<80;i++){ t+=100; m.flow_.in=(t-1000)*0.003f; m.flow_.out=m.flow_.in*0.1f; m.update(); }   // 8 s at 3 ml/s, 10 % back
  CHECK(m.extraction_.state()==EXTRACTION_MANUAL);                                   // far above the target (40): no stop
  uint16_t vol=m.extraction_.volumeMl(); CHECK(vol>=20 && vol<=22 && m.extraction_.durationSeconds()>=7);
  m.handleShortPush();                                   // stop
  CHECK(m.extraction_.state()==EXTRACTION_FINISHED && !PUMPON && pins[16]==0);
  calls.clear(); unsigned long t0=t;
  while(t<t0+9900){ t+=100; m.update(); }
  CHECK(!has("dur(0,0,0,1)"));                           // the values stay 10 s
  while(t<t0+10300){ t+=100; m.update(); }
  CHECK(has("dur(0,0,0,1)") && has("vol(40,1,0,1)"));    // then the target volume and 0 s
  m.handleShortPush(); CHECK(m.extraction_.state()==EXTRACTION_MANUAL);              // ready again
  m.handleShortPush();                                   // stop: the values of this extraction are shown
  t+=100; m.update(); calls.clear();
  m.handleVeryLongPush();                                // MANUAL -> AUTO: the values are reset at once
  t+=100; m.update(); CHECK(has("dur(0,0,0,1)") && has("vol(40,1,0,1)"));
  bool letterA=has("drop(0,1,0,0)");
  calls.clear(); t+=100; m.update(); while(t<t0+40000){ t+=100; m.update(); } CHECK(!has("dur(0,0,0,1)"));   // and not again later
  CHECK(!m.screen_.settings().manual && EEPROM.mem[8]==0 && letterA);
  m.handleShortPush(); CHECK(m.extraction_.state()==EXTRACTION_RUNNING);              // AUTO extraction again
  printf("manual mode: ok\n");
}
// MANUAL time: the parameters of the mode, the countdown, the stop by itself
static void manualTime(){
  memset(EEPROM.mem,0xFF,64); SettingsStore st; st.load(); st.savePreinfusion(0); st.saveFlush(0); st.saveManual(true);
  CoffeeMachine m; t=1000; m.begin(); calls.clear();
  CHECK(m.screen_.settings().manual && m.screen_.activeParameter()==PARAMETER_VOLUME);
  // MANUAL: volume -> MANUAL time -> temperature -> volume (pre-infusion, wait and flush are skipped)
  m.handleDoublePush(); CHECK(m.screen_.activeParameter()==PARAMETER_MANUAL_TIME);
  calls.clear(); m.screen_.adjustActiveParameter(7); t+=100; m.update(); CHECK(has("dur(7,1,0,1)") || has("dur(7,1,0,0)"));   // large digits, no tag
  m.handleLongPush(); CHECK(EEPROM.mem[9]==7);           // saved
  m.handleDoublePush(); CHECK(m.screen_.activeParameter()==PARAMETER_TEMPERATURE);
  m.handleDoublePush(); CHECK(m.screen_.activeParameter()==PARAMETER_VOLUME);
  // AUTO: the MANUAL time is skipped, the others are back
  m.handleVeryLongPush(); CHECK(!m.screen_.settings().manual);
  for(int i=0;i<5;i++){ m.handleDoublePush(); CHECK(m.screen_.activeParameter()!=PARAMETER_MANUAL_TIME); }
  CHECK(m.screen_.activeParameter()==PARAMETER_VOLUME);
  m.handleDoublePush(); m.handleDoublePush();            // pre-infusion, wait
  m.handleVeryLongPush();                                // back to MANUAL: the volume is selected again
  CHECK(m.screen_.settings().manual && m.screen_.activeParameter()==PARAMETER_VOLUME);
  // the extraction: the time counts down from 7 to 0, then the pump stops by itself
  calls.clear(); m.handleShortPush(); CHECK(m.extraction_.state()==EXTRACTION_MANUAL);
  unsigned long t0=t; bool seen7=false, seen1=false;
  for(int i=0;i<90;i++){ t+=100; m.flow_.in+=0.3f; m.update();
    if(has("dur(7,0,0,0)")||has("dur(7,0,0,1)")) seen7=true;
    if(has("dur(1,0,0,0)")||has("dur(1,0,0,1)")) seen1=true;
    if(m.extraction_.state()!=EXTRACTION_MANUAL) break; }
  CHECK(seen7 && seen1);
  CHECK(m.extraction_.state()==EXTRACTION_FINISHED && !PUMPON && pins[16]==0);
  CHECK(t-t0>=6900 && t-t0<=7300);
  calls.clear(); t+=100; m.update();
  // the final values: elapsed time (7 s), kept 10 s
  { calls.clear(); for(int i=0;i<50;i++){ t+=100; m.update(); } CHECK(!has("dur(0,0,0,1)") && !has("dur(7,0,0,1)"));   // the values of the extraction stay
    calls.clear(); for(int i=0;i<60;i++){ t+=100; m.update(); }
    CHECK(has("dur(7,0,0,1)") && !has("dur(0,0,0,1)")); }                  // then the MANUAL time (7 s), not 0
  // the time set stays shown when another parameter is selected, and after a mode change
  m.handleDoublePush(); CHECK(m.screen_.activeParameter()==PARAMETER_MANUAL_TIME); m.screen_.adjustActiveParameter(3);
  calls.clear(); m.handleDoublePush(); t+=100; m.update(); CHECK(has("dur(10,0,0,1)"));
  calls.clear(); m.handleVeryLongPush(); m.handleVeryLongPush(); t+=100; m.update(); CHECK(has("dur(10,0,0,1)"));   // AUTO then MANUAL again
  m.handleDoublePush(); m.screen_.adjustActiveParameter(-10); m.handleLongPush(); m.handleDoublePush(); m.handleDoublePush();
  // time 0: the old behaviour, the pump runs until the second push, and 0 s is shown
  CHECK(EEPROM.mem[9]==0 && m.screen_.activeParameter()==PARAMETER_VOLUME);
  m.handleShortPush(); CHECK(m.extraction_.state()==EXTRACTION_MANUAL);
  for(int i=0;i<300;i++){ t+=100; m.flow_.in+=0.3f; m.update(); } CHECK(m.extraction_.state()==EXTRACTION_MANUAL && PUMPON);
  printf("manual time: ok\n");
}
int main(){ scenario(0); scenario(3); noWater(); temperatureError(); manualMode(); manualTime(); printf(fails?"%d FAIL\n":"ALL OK\n",fails); return fails!=0; }
