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
static const unsigned long RESULT_MS = EXTRACTION_RESULT_DISPLAY_MS;   // how long the values of an extraction stay on the screen (Config.h)
// The MAX31865 answers the RTD register: the probe is at 93 degrees (PT100, Callendar-Van Dusen) unless a test sets another one
static uint16_t rtdRegister=0; static int byteIndex=0;
static uint8_t answer(uint8_t v){ if(v==0x01){ byteIndex=0; return 0; } if(v!=0) { byteIndex=-1; return 0; }
  if(byteIndex<0) return 0; return byteIndex++==0 ? (uint8_t)(rtdRegister>>8) : (uint8_t)rtdRegister; }
static void probeAt(double celsius){
  const double A=3.9083e-3,B=-5.775e-7; const double r=RNOMINAL*(1+A*celsius+B*celsius*celsius);
  rtdRegister=(uint16_t)(((uint16_t)(r/RREF*32768.0+0.5))<<1);
}
static unsigned long welcomeAt=0, welcomeEnd=0;   // the clock when the welcome picture starts and ends
std::vector<std::string> calls;
static void L(const char*f,double a=0,int b=0,int c=0,int d=0){char s[80];snprintf(s,80,"%s(%g,%d,%d,%d)",f,a,b,c,d);calls.push_back(s);}
void beginDisplay(){} void showDisplay(){} void setBacklight(uint8_t v){L("backlight",v);} void beginWelcome(){L("welcome"); welcomeAt=t;} void endWelcome(){L("endwelcome"); welcomeEnd=t;} void showStandbyPicture(){L("standbypic");} void drawIcons(){L("icons");}
void drawTemperature(double v,TextSize s){L("temp",v,s);} void drawVolume(uint16_t v,TextSize s,PhaseTag g,bool r,uint8_t p){if(p==DISPLAY_ROLL_DONE)L("vol",v,s,g,r);}
void drawDuration(uint16_t v,TextSize s,PhaseTag g,bool r,uint8_t p){if(p==DISPLAY_ROLL_DONE)L("dur",v,s,g,r);}
void clearTemperature(){} void clearVolume(){} void clearDuration(){} void setTextBrightness(uint8_t){} void forgetField(uint8_t){} void repaintField(uint8_t){} void drawHeaterIndicator(bool,bool){} void drawPhaseTag(PhaseTag g,bool v){L("tag",0,g,v);} void drawDrop(bool v,bool m){L("drop",m,v);} void drawModeLetter(bool){} void drawDropFrame(bool){} void drawTemperatureError(){L("temperr");}
void drawFlowLimit(uint16_t,bool){} void drawSteamIcon(){L("steamicon");} void clearScreen(){L("clearscreen");} void drawSteamTemperature(double v,bool e){L("steam",v,e);}

#define PUMPON (pins[PIN_PUMP_RELAY]==1)
static bool has(const char*s){ for(auto&c:calls) if(c==s) return true; return false; }
static void step(CoffeeMachine&m,unsigned long dt){ t+=dt; m.update(); }
// The settings are written SETTINGS_SAVE_DELAY_MS after the last change, when nothing is going on: let that time pass
static void settle(CoffeeMachine&m){ step(m,SETTINGS_SAVE_DELAY_MS+10); for(int i=0;i<5;i++) step(m,20); }
static void scenario(int flush){
  memset(EEPROM.mem,0xFF,64); SettingsStore st; st.load(); SAVE(st,preinfusion,0); SAVE(st,preinfusionWait,0); SAVE(st,flush,flush);
  CoffeeMachine m; t=1000; m.begin(); calls.clear();
  m.handleStartButton();                          // start the extraction (volume selected)
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
    m.handleStartButton(); CHECK(m.extraction_.state()==EXTRACTION_FLUSHING);
    { ActiveParameter p=m.screen_.activeParameter(); m.handleDoublePush();      // flush: a double push skips it
      CHECK(!m.extraction_.isActive() && m.screen_.activeParameter()!=p); CHECK(pins[PIN_PUMP_RELAY]==0 && pins[PIN_VALVE_RELAY]==0); }
    for(int i=0;i<8 && m.screen_.activeParameter()!=PARAMETER_VOLUME;i++) m.screen_.selectNextParameter();   // back to volume
    CHECK(m.screen_.activeParameter()==PARAMETER_VOLUME);
    m.handleStartButton(); { unsigned long ts=t; for(int i=0;i<100;i++){ t+=100; m.flow_.vol=(t-ts)/1000.0f*10; m.update(); if(m.extraction_.state()==EXTRACTION_FLUSH_WAIT) break; } }
    m.handleStartButton(); CHECK(m.extraction_.state()==EXTRACTION_FLUSHING);
    calls.clear(); unsigned long tf=t;
    for(int i=0;i<100;i++){ t+=100; m.update(); if(!m.extraction_.isActive()) break; }
    CHECK(m.extraction_.state()==EXTRACTION_FINISHED);
    // at the end of the flush the values of the extraction are shown again, not the target yet
    CHECK(!has("dur(0,0,0,1)"));   // (the volume ends on the target: it looks like the target, only the duration tells)
    // they stay EXTRACTION_RESULT_DISPLAY_MS from the end of the flush, then the target volume (40, large
    // because the volume is selected) and 0 s come back
    calls.clear(); unsigned long tfe=t;
    while(t<tfe+RESULT_MS-3200){ t+=100; m.update(); }
    while(t<tfe+RESULT_MS-100){ t+=100; m.update(); }
    CHECK(!has("dur(0,0,0,1)"));                     // still held, no blinking
    while(t<tfe+RESULT_MS+200){ t+=100; m.update(); }
    CHECK(has("vol(40,1,0,1)")); CHECK(has("dur(0,0,0,1)"));
    printf("flush=%d: values reset after the end of the flush\n",flush);
  } else {
    CHECK(m.extraction_.state()==EXTRACTION_FINISHED);
    // final values shown, held EXTRACTION_RESULT_DISPLAY_MS
    bool final=false; for(auto&c:calls) if(c.rfind("dur(",0)==0 && c!="dur(0,0,0,1)") final=true; CHECK(final);
    calls.clear(); unsigned long t0=t;
    while(t<t0+RESULT_MS-1700){ t+=100; m.update(); }
    while(t<t0+RESULT_MS-100){ t+=100; m.update(); }
    CHECK(!has("dur(0,0,0,1)"));     // still holding 0.1 s before the end
    while(t<t0+RESULT_MS+200){ t+=100; m.update(); }
    CHECK(has("dur(0,0,0,1)")); CHECK(has("vol(40,1,0,1)"));       // reset
    calls.clear(); { unsigned long t1=t; while(t<t1+3000){ t+=100; m.update(); } }
    // a new extraction during the hold cancels the reset
    m.handleStartButton(); t+=100; m.update(); calls.clear();
    for(int i=0;i<30;i++){ t+=100; m.flow_.vol=1; m.update(); }
    CHECK(m.extraction_.state()==EXTRACTION_RUNNING);
    m.handleStartButton();   // stop it
    t0=t; calls.clear(); while(t<t0+4000){ t+=100; m.update(); }
    CHECK(!has("dur(0,0,0,1)"));
    m.handleStartButton(); t+=100; m.update();     // start again: cancels the pending hold
    printf("flush=%d: hold 10 s ok\n",flush);
  }
}
// No-water error: the drop blinks every DROP_BLINK_MS until a long push; short / double pushes do nothing
static void noWater(){
  memset(EEPROM.mem,0xFF,64); SettingsStore st; st.load(); SAVE(st,preinfusion,0); SAVE(st,preinfusionWait,0); SAVE(st,flush,0);
  CoffeeMachine m; t=1000; m.begin(); calls.clear();
  m.handleStartButton(); CHECK(m.extraction_.state()==EXTRACTION_RUNNING);
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
  // the start / stop button and the double push are ignored
  m.handleStartButton(); m.handleDoublePush(); t+=100; m.update();
  CHECK(m.extraction_.state()==EXTRACTION_ERROR_NO_WATER && !PUMPON);
  // the short push of the encoder clears the error (the long push is disabled): the drop is shown and stops blinking, nothing is started
  m.handleShortPush(); calls.clear(); t+=100; m.update();
  CHECK(m.extraction_.state()==EXTRACTION_IDLE && !m.extraction_.hasNoWaterError());
  bool hidden=false; calls.clear(); for(int i=0;i<30;i++){ t+=100; m.update(); } CHECK(!has("drop(0,0,0,0)"));
  (void)hidden;
  m.handleStartButton(); CHECK(m.extraction_.state()==EXTRACTION_RUNNING);              // a new extraction is possible
  printf("no-water: ok\n");
}
// Temperature sensor fault: "ERR" replaces the temperature, which comes back with the sensor
static void temperatureError(){
  ScreenController sc; Settings in={93,40,10,5,5}; sc.begin(in); sc.showLiveTemperature(0,91.5); calls.clear(); sc.update(0);
  CHECK(!has("temperr(0,0,0,0)"));
  sc.showTemperatureError(); calls.clear(); sc.update(1); CHECK(has("temperr(0,0,0,0)"));
  sc.showTemperatureError(); calls.clear(); sc.update(2); CHECK(calls.empty());             // drawn once
  // a double push while ERR is on the screen: that field does not change, so ERR is not erased and drawn again (flicker)
  sc.selectNextParameter(); calls.clear(); sc.update(2); CHECK(!has("temperr(0,0,0,0)"));
  sc.selectNextParameter(); calls.clear(); sc.update(2); CHECK(!has("temperr(0,0,0,0)"));
  // the setpoint is still shown while it is edited
  while(sc.activeParameter()!=PARAMETER_TEMPERATURE) sc.selectNextParameter(); CHECK(sc.activeParameter()==PARAMETER_TEMPERATURE);
  calls.clear(); sc.update(3); CHECK(!has("temperr(0,0,0,0)"));
  sc.selectNextParameter(); calls.clear(); sc.update(4); CHECK(has("temperr(0,0,0,0)"));     // back to the live value: still ERR
  sc.showLiveTemperature(5000,90.0); calls.clear(); sc.update(5000); CHECK(!has("temperr(0,0,0,0)") && !calls.empty());
  printf("temperature error: ok\n");
}
// MANUAL mode: a long push switches the mode (saved), a push starts the pump, the next one stops it
static void manualMode(){
  memset(EEPROM.mem,0xFF,64); SettingsStore st; st.load(); SAVE(st,preinfusion,0); SAVE(st,flush,0);
  CoffeeMachine m; t=1000; m.begin(); calls.clear();
  CHECK(!m.screen_.settings().manual);
  m.handlePush(PUSH_SHORT); CHECK(m.screen_.settings().manual);   // the short push switches the mode: AUTO -> MANUAL
  CHECK(EEPROM.mem[8]==0); settle(m);                    // saved SETTINGS_SAVE_DELAY_MS later
  CHECK(m.screen_.settings().manual && EEPROM.mem[8]==1 && has("drop(1,1,0,0)"));   // letter M, drop shown
  { CoffeeMachine m2; m2.begin(); CHECK(m2.screen_.settings().manual); }            // remembered after a restart
  m.handleStartButton();                                   // start the pump
  CHECK(m.extraction_.state()==EXTRACTION_MANUAL && PUMPON && pins[PIN_VALVE_RELAY]==1);
  m.handleShortPush(); CHECK(m.screen_.settings().manual && m.extraction_.state()==EXTRACTION_MANUAL);   // ignored while running
  m.handleDoublePush(); CHECK(m.extraction_.state()==EXTRACTION_MANUAL);
  calls.clear(); const unsigned long tStart=t;
  for(int i=0;i<80;i++){ t+=100; m.flow_.in=(t-tStart)*0.003f; m.flow_.out=m.flow_.in*0.1f; m.update(); }   // 8 s at 3 ml/s, 10 % back
  CHECK(m.extraction_.state()==EXTRACTION_MANUAL);                                   // far above the target (40): no stop
  uint16_t vol=m.extraction_.volumeMl(); CHECK(vol>=20 && vol<=22 && m.extraction_.durationSeconds()>=7);
  m.handleStartButton();                                   // stop
  CHECK(m.extraction_.state()==EXTRACTION_FINISHED && !PUMPON && pins[PIN_VALVE_RELAY]==0);
  calls.clear(); unsigned long t0=t;
  while(t<t0+RESULT_MS-1700){ t+=100; m.update(); }
  while(t<t0+RESULT_MS-100){ t+=100; m.update(); }
  CHECK(!has("dur(0,0,0,1)"));                           // the values stay as long as EXTRACTION_RESULT_DISPLAY_MS
  while(t<t0+RESULT_MS+300){ t+=100; m.update(); }
  CHECK(has("dur(0,0,0,1)") && has("vol(0,1,0,1)") && !has("vol(40,1,0,1)"));    // then 0 ml (no target in MANUAL) and 0 s
  CHECK(!m.screen_.adjustActiveParameter(5) && m.screen_.settings().volume==DEFAULT_VOLUME);   // the volume is not a setting here
  m.handleStartButton(); CHECK(m.extraction_.state()==EXTRACTION_MANUAL);              // ready again
  m.handleStartButton();                                   // stop: the values of this extraction are shown
  t+=100; m.update(); calls.clear();
  m.handleShortPush();                                // MANUAL -> AUTO: the values are reset at once
  t+=100; m.update(); CHECK(has("dur(0,0,0,1)") && has("vol(40,1,0,1)"));
  bool letterA=has("drop(0,1,0,0)");
  calls.clear(); t+=100; m.update(); while(t<t0+40000){ t+=100; m.update(); } CHECK(!has("dur(0,0,0,1)"));   // and not again later
  CHECK(!m.screen_.settings().manual && EEPROM.mem[8]==0 && letterA);
  m.handleStartButton(); CHECK(m.extraction_.state()==EXTRACTION_RUNNING);              // AUTO extraction again
  printf("manual mode: ok\n");
}
// MANUAL time: the parameters of the mode, the countdown, the stop by itself
static void manualTime(){
  memset(EEPROM.mem,0xFF,64); SettingsStore st; st.load(); SAVE(st,preinfusion,0); SAVE(st,flush,0); SAVE(st,manual,true);
  CoffeeMachine m; t=1000; m.begin(); calls.clear();
  CHECK(m.screen_.settings().manual && m.screen_.activeParameter()==PARAMETER_VOLUME);
  // MANUAL: volume -> MANUAL time -> temperature -> volume (pre-infusion, wait and flush are skipped)
  m.handleDoublePush(); CHECK(m.screen_.activeParameter()==PARAMETER_MANUAL_TIME);
  calls.clear(); m.screen_.adjustActiveParameter(7); t+=100; m.update(); CHECK(has("dur(7,1,0,1)") || has("dur(7,1,0,0)"));   // large digits, no tag
  CHECK(EEPROM.mem[9]==0);                               // not saved while it is edited
  m.handleDoublePush(); CHECK(m.screen_.activeParameter()==PARAMETER_TEMPERATURE);
  CHECK(EEPROM.mem[9]==0); settle(m);
  CHECK(EEPROM.mem[9]==7);                               // saved by the double push that leaves it (a moment later)
  m.handleDoublePush(); CHECK(m.screen_.activeParameter()==PARAMETER_VOLUME);
  // AUTO: the MANUAL time is skipped, the others are back
  m.handleShortPush(); CHECK(!m.screen_.settings().manual);
  for(int i=0;i<6;i++){ m.handleDoublePush(); CHECK(m.screen_.activeParameter()!=PARAMETER_MANUAL_TIME); }
  CHECK(m.screen_.activeParameter()==PARAMETER_VOLUME);
  m.handleDoublePush(); m.handleDoublePush(); m.handleDoublePush();   // pre-infusion, limit, wait
  m.handleShortPush();                                // back to MANUAL: the volume is selected again
  CHECK(m.screen_.settings().manual && m.screen_.activeParameter()==PARAMETER_VOLUME);
  // the extraction: the time counts down from 7 to 0, then the pump stops by itself
  calls.clear(); m.handleStartButton(); CHECK(m.extraction_.state()==EXTRACTION_MANUAL);
  unsigned long t0=t; bool seen7=false, seen1=false;
  for(int i=0;i<90;i++){ t+=100; m.flow_.in+=0.3f; m.update();
    if(has("dur(7,0,0,0)")||has("dur(7,0,0,1)")) seen7=true;
    if(has("dur(1,0,0,0)")||has("dur(1,0,0,1)")) seen1=true;
    if(m.extraction_.state()!=EXTRACTION_MANUAL) break; }
  CHECK(seen7 && seen1);
  CHECK(m.extraction_.state()==EXTRACTION_FINISHED && !PUMPON && pins[PIN_VALVE_RELAY]==0);
  CHECK(t-t0>=6900 && t-t0<=7300);
  calls.clear(); t+=100; m.update();
  // the final values: elapsed time (7 s), kept EXTRACTION_RESULT_DISPLAY_MS
  { calls.clear(); for(unsigned i=0;i<RESULT_MS/200;i++){ t+=100; m.update(); } CHECK(!has("dur(0,0,0,1)") && !has("dur(7,0,0,1)"));   // the values of the extraction stay
    calls.clear(); for(unsigned i=0;i<RESULT_MS/200+10;i++){ t+=100; m.update(); }
    CHECK(has("dur(7,0,0,1)") && !has("dur(0,0,0,1)") && has("vol(0,1,0,1)")); }   // then 0 ml and the MANUAL time (7 s), not 0
  // the time set stays shown when another parameter is selected, and after a mode change
  m.handleDoublePush(); CHECK(m.screen_.activeParameter()==PARAMETER_MANUAL_TIME); m.screen_.adjustActiveParameter(3);
  calls.clear(); m.handleDoublePush(); t+=100; m.update(); CHECK(has("dur(10,0,0,1)"));
  calls.clear(); m.handleShortPush(); m.handleShortPush(); t+=100; m.update(); CHECK(has("dur(10,0,0,1)"));   // AUTO then MANUAL again
  m.handleDoublePush(); m.screen_.adjustActiveParameter(-10); m.handleDoublePush(); m.handleDoublePush();
  // time 0: the old behaviour, the pump runs until the second push, and 0 s is shown
  settle(m);
  CHECK(EEPROM.mem[9]==0 && m.screen_.activeParameter()==PARAMETER_VOLUME);
  m.handleStartButton(); CHECK(m.extraction_.state()==EXTRACTION_MANUAL);
  for(int i=0;i<300;i++){ t+=100; m.flow_.in+=0.3f; m.update(); } CHECK(m.extraction_.state()==EXTRACTION_MANUAL && PUMPON);
  printf("manual time: ok\n");
}
// Automatic saving: at a double push, at the start of an extraction (the volume) and at a change of mode,
// and only what changed is written
static void autoSave(){
  memset(EEPROM.mem,0xFF,64); SettingsStore st; st.load(); SAVE(st,preinfusion,0); SAVE(st,flush,0);
  CoffeeMachine m; t=1000; m.begin(); calls.clear();
  int16_t v; auto savedVolume=[&](){ EEPROM.get(3,v); return v; };
  auto savedTemperature=[&](){ int16_t x; EEPROM.get(1,x); return x; };
  int w=EEPROM.writes;
  // volume: edited, not saved, then saved by the start of the extraction
  CHECK(m.screen_.activeParameter()==PARAMETER_VOLUME);
  m.screen_.adjustActiveParameter(5); CHECK(savedVolume()==DEFAULT_VOLUME && EEPROM.writes==w);
  m.handleStartButton(); CHECK(savedVolume()==DEFAULT_VOLUME+5 && EEPROM.writes==w+1 && m.extraction_.isActive());
  m.extraction_.stop(EXTRACTION_FINISHED); w=EEPROM.writes;
  // a start without change writes nothing
  m.handleStartButton(); CHECK(EEPROM.writes==w); m.extraction_.stop(EXTRACTION_FINISHED);
  // temperature: edited, applied and saved when the double push leaves it
  m.handleDoublePush(); m.handleDoublePush(); m.handleDoublePush(); m.handleDoublePush();   // volume -> pre -> limit -> wait -> flush
  m.handleDoublePush(); CHECK(m.screen_.activeParameter()==PARAMETER_TEMPERATURE); w=EEPROM.writes;
  m.screen_.adjustActiveParameter(-3); CHECK(savedTemperature()==DEFAULT_TEMPERATURE && EEPROM.writes==w);
  int ops=EEPROM.blockOps;
  m.handleDoublePush(); CHECK(savedTemperature()==DEFAULT_TEMPERATURE && EEPROM.blockOps==ops);   // not at once...
  step(m,SETTINGS_SAVE_DELAY_MS-200); CHECK(EEPROM.blockOps==ops);                                // ...not before the delay...
  m.screen_.adjustActiveParameter(1);                                                             // (what is edited next is not saved with it)
  step(m,300); step(m,20); CHECK(savedTemperature()==DEFAULT_TEMPERATURE-3 && EEPROM.writes==w+1 && EEPROM.blockOps==ops+1);   // ...then ONE block operation
  for(int i=0;i<100;i++) step(m,20); CHECK(EEPROM.blockOps==ops+1);                               // and nothing more
  // double pushes without any change write nothing
  m.screen_.adjustActiveParameter(-1); m.handleDoublePush(); settle(m);
  w=EEPROM.writes; ops=EEPROM.blockOps; for(int i=0;i<10;i++) m.handleDoublePush(); settle(m); CHECK(EEPROM.writes==w && EEPROM.blockOps==ops);
  // a change of mode saves the mode, and what was being edited: the two saves of one push are ONE block operation
  m.screen_.adjustActiveParameter(2); w=EEPROM.writes; m.handleShortPush(); settle(m);
  CHECK(EEPROM.mem[8]==1 && EEPROM.writes>=w+1 && EEPROM.blockOps==ops+1);
  w=EEPROM.writes; m.handleShortPush(); settle(m); CHECK(EEPROM.mem[8]==0 && EEPROM.writes==w+1 && EEPROM.blockOps==ops+2);
  // the store itself: the 22 bytes are ONE block operation, nothing when nothing changed; the stop delay too
  { SettingsStore s; Settings x=s.load(); const int o=EEPROM.blockOps;
    s.save(x); CHECK(EEPROM.blockOps==o);
    x.volume=x.volume+1; x.temperature=x.temperature+1; x.manual=!x.manual; x.loss=x.loss+1.5f; x.flowLimit=20; s.save(x); CHECK(EEPROM.blockOps==o+1);   // five fields, one operation
    s.save(x); CHECK(EEPROM.blockOps==o+1);
    Settings y=s.load(); CHECK(y.volume==x.volume && y.temperature==x.temperature && y.manual==x.manual && y.loss==x.loss && y.flowLimit==20);
    s.saveStopDelay(0.42f); CHECK(EEPROM.blockOps==o+2); s.saveStopDelay(0.42f); CHECK(EEPROM.blockOps==o+2 && s.loadStopDelay()==0.42f);
    s.save(x); CHECK(EEPROM.blockOps==o+2 && s.loadStopDelay()==0.42f); }   // the settings keep the stop delay as it is
  printf("auto save: ok\n");
}
// Calibration of the loss: CAL appears after a normal AUTO extraction, the cup weight corrects the loss
static float savedLoss(){ float f; memcpy(&f,EEPROM.mem+10,4); return f; }
static void runAuto(CoffeeMachine&m){   // 3 ml/s, nothing back through the OPV, until the end of the extraction
  m.handleStartButton(); unsigned long t0=t;
  for(int i=0;i<600 && m.extraction_.isActive();i++){ t+=100; m.flow_.in=(t-t0)*0.003f; m.flow_.out=0; m.update(); }
  t+=100; m.update();
  for(int i=0;i<25;i++){ t+=100; m.update(); }   // the water that comes after the stop is counted (STOP_SETTLE_MS), then CAL is offered
}
static void gotoParam(CoffeeMachine&m,ActiveParameter p){ for(int i=0;i<8 && m.screen_.activeParameter()!=p;i++) m.handleDoublePush(); }
static void calibration(){
  memset(EEPROM.mem,0xFF,64); SettingsStore st; st.load(); SAVE(st,preinfusion,0); SAVE(st,preinfusionWait,0); SAVE(st,flush,0);
  CoffeeMachine m; t=1000; m.begin();
  CHECK(savedLoss()==DEFAULT_LOSS_ML);                    // written by the defaults
  runAuto(m);
  CHECK(m.extraction_.state()==EXTRACTION_FINISHED && m.extraction_.calibratable());
  float net=m.extraction_.netVolumeMl(); CHECK(net>=DEFAULT_LOSS_ML+40 && net<DEFAULT_LOSS_ML+43);
  CHECK(m.screen_.calibrationAvailable_);
  // the 10 s of the display of the result pass: CAL does not go away, and what was entered is not touched
  { const int16_t estimate=m.screen_.calibrationValue_; gotoParam(m,PARAMETER_CALIBRATION); m.screen_.adjustActiveParameter(-3);
    calls.clear(); unsigned long tr=t; while(t<tr+EXTRACTION_RESULT_DISPLAY_MS+5000){ t+=100; m.update(); }
    CHECK(m.screen_.calibrationAvailable_ && m.screen_.activeParameter()==PARAMETER_CALIBRATION && m.screen_.calibrationValue_==estimate-3);
    for(auto&c:calls) CHECK(c.rfind("vol(",0)!=0 || c.find(",1,0,")==std::string::npos);   // no vol(...) of the target (no tag) over the CAL one
    m.screen_.adjustActiveParameter(3); }
  gotoParam(m,PARAMETER_CALIBRATION); CHECK(m.screen_.activeParameter()==PARAMETER_CALIBRATION);
  { char b[40]; snprintf(b,40,"vol(%d,1,%d,0)",m.screen_.calibrationValue_,(int)PHASE_TAG_CALIBRATION);   // (the tag is already drawn)
    calls.clear(); t+=100; m.update(); CHECK(has(b)); }
  CHECK(m.screen_.calibrationValue_==m.extraction_.volumeMl());       // starts at the estimate
  m.screen_.adjustActiveParameter(-(m.screen_.calibrationValue_-30)); // the cup really holds 30 g
  m.handleDoublePush();                                  // validates
  settle(m);
  { Journal jj; JournalEntry je; CHECK(jj.read(0,je) && je.cal==30 && je.reason==END_TARGET); }   // the weight is in the journal (last entry)
  float expect=DEFAULT_LOSS_ML+CALIBRATION_GAIN*((net-30)-DEFAULT_LOSS_ML);
  CHECK(m.screen_.settings().loss>expect-0.01f && m.screen_.settings().loss<expect+0.01f);
  CHECK(savedLoss()>expect-0.01f && savedLoss()<expect+0.01f);        // saved
  CHECK(m.screen_.activeParameter()==PARAMETER_VOLUME && !m.screen_.calibrationAvailable_);   // modified: back to the volume
  { CoffeeMachine m2; m2.begin(); CHECK(m2.screen_.settings().loss>expect-0.01f && m2.screen_.settings().loss<expect+0.01f); }
  gotoParam(m,PARAMETER_VOLUME);
  for(int i=0;i<8;i++){ m.handleDoublePush(); CHECK(m.screen_.activeParameter()!=PARAMETER_CALIBRATION); }   // gone from the cycle
  gotoParam(m,PARAMETER_VOLUME);
  // the next extraction stops with the new loss (net volume = 40 + loss)
  runAuto(m); net=m.extraction_.netVolumeMl(); CHECK(net>=40+expect-0.5f && net<40+expect+3);
  // CAL not touched: the loss stays
  gotoParam(m,PARAMETER_CALIBRATION); float before=m.screen_.settings().loss; m.handleDoublePush();
  CHECK(m.screen_.settings().loss==before && m.screen_.activeParameter()==PARAMETER_PREINFUSION);   // not modified: the cycle goes on
  { Journal jj; JournalEntry je; CHECK(jj.read(0,je) && je.cal==0 && jj.read(1,je) && je.cal==30); }   // not modified: nothing written, the older one keeps its weight
  // an extraction under CALIBRATION_MIN_ML is not offered
  gotoParam(m,PARAMETER_VOLUME); m.screen_.adjustActiveParameter(5-m.screen_.settings().volume); runAuto(m);
  CHECK(m.extraction_.state()==EXTRACTION_FINISHED && !m.extraction_.calibratable());
  gotoParam(m,PARAMETER_FLUSH); for(int i=0;i<2;i++) m.handleDoublePush(); CHECK(m.screen_.activeParameter()!=PARAMETER_CALIBRATION);
  // MANUAL extraction: not offered
  m.handleShortPush(); m.handleStartButton(); for(int i=0;i<60;i++){ t+=100; m.flow_.in=m.flow_.in+0.3f; m.update(); }
  m.handleStartButton(); t+=100; m.update(); CHECK(!m.extraction_.calibratable() && !m.screen_.calibrationAvailable_);
  // the clamp and the validity of the EEPROM value
  { ScreenController sc; Settings in={93,40,10,5,5,0,50.0f,false}; sc.begin(in); sc.offerCalibration(40); sc.selectNextParameter();
    for(int i=0;i<8 && sc.activeParameter()!=PARAMETER_CALIBRATION;i++) sc.selectNextParameter();
    sc.adjustActiveParameter(-40);   // 0 g in the cup: measured loss = net, 100 -> clamp
    CHECK(sc.applyCalibration(300.0f)==LOSS_MAX_ML); }
  memset(EEPROM.mem,0xFF,64); { SettingsStore s2; s2.load(); }
  float bad=1000.0f; memcpy(EEPROM.mem+10,&bad,4); { SettingsStore s2; CHECK(s2.load().loss==DEFAULT_LOSS_ML); }
  bad=-3.0f; memcpy(EEPROM.mem+10,&bad,4); { SettingsStore s2; CHECK(s2.load().loss==DEFAULT_LOSS_ML); }
  bad=42.5f; memcpy(EEPROM.mem+10,&bad,4); { SettingsStore s2; CHECK(s2.load().loss==42.5f); }
  { memset(EEPROM.mem,0xFF,64); CoffeeMachine m3; t=1000; m3.begin(); CHECK(m3.heater_.setpoint_==DEFAULT_TEMPERATURE+TEMPERATURE_OFFSET); }   // regulated on the probe
  printf("calibration: ok\n");
}
static bool startsWith(const std::string &s, const char *prefix);
static int count(const char*name){ int n=0; for(auto&c:calls) if(c==name) n++; return n; }
static void standby(){
  memset(EEPROM.mem,0xFF,64); CoffeeMachine m; t=1000; m.begin(); calls.clear();
  unsigned long t0=t; bool before=false;
  while(t<t0+STANDBY_MINUTES*60000UL-2000){ t+=1000; m.update(); before=before||m.standby_; }
  CHECK(!before && !m.standby_);
  CHECK(count("standbypic(0,0,0,0)")==0);                         // the screen is the normal one until then
  while(t<t0+STANDBY_MINUTES*60000UL+3000){ t+=1000; m.update(); }
  CHECK(m.standby_ && !m.heater_.isOn());                         // heater off
  CHECK(count("standbypic(0,0,0,0)")==1);                         // the welcome picture replaces the screen, once
  // during the standby nothing is drawn over the picture, even when what is shown changes
  calls.clear();
  for(int i=0;i<30;i++){ t+=1000; m.screen_.showLiveTemperature(t,80.0+i); m.screen_.showReady(); m.update(); }   // showReady(): the volume and the duration are redrawn if anything is drawn
  CHECK(calls.empty());
  // the wake-up: the backlight goes off, the picture is cleared in the dark, the icons, the drop and every field are drawn again, then the backlight
  m.wakeUp(); CHECK(!m.standby_);
  {
    const int cleared=count("clearscreen(0,0,0,0)")>0 ? (int)(std::find(calls.begin(),calls.end(),"clearscreen(0,0,0,0)")-calls.begin()) : -1;
    CHECK(calls[0]=="backlight(0,0,0,0)" && cleared==1 && count("icons(0,0,0,0)")==1 && count("backlight(255,0,0,0)")==1 && calls.back()=="backlight(255,0,0,0)");
    bool temperature=false, volume=false, duration=false;
    for(size_t i=0;i<calls.size();i++){ temperature|=startsWith(calls[i],"temp(")||startsWith(calls[i],"temperr("); volume|=startsWith(calls[i],"vol("); duration|=startsWith(calls[i],"dur("); }
    CHECK(temperature && volume && duration);
  }
  m.wakeUp(); calls.clear(); m.wakeUp(); CHECK(calls.empty());     // already awake: nothing happens
  // and the standby comes again, then an extraction (not the encoder) wakes it too
  calls.clear();
  unsigned long t1=t; while(t<t1+STANDBY_MINUTES*60000UL+3000){ t+=1000; m.update(); }
  CHECK(m.standby_ && count("standbypic(0,0,0,0)")==1);
  calls.clear(); m.handleStartButton(); t+=100; m.update(); CHECK(!m.standby_ && m.extraction_.isActive());   // an extraction wakes it
  CHECK(count("clearscreen(0,0,0,0)")==1 && count("backlight(255,0,0,0)")==1);   // the real screen came back
  CoffeeMachine m2; m2.begin(); CHECK(!m2.standby_);               // nothing is remembered
  printf("standby: ok\n");
}
// Start-up: the backlight is switched on once everything is drawn (the icons, the letter of the mode and the three
// values), so that the screen appears complete, not the icons first and the values a moment later
static bool startsWith(const std::string &s, const char *prefix){ return s.rfind(prefix,0)==0; }
static void startup(){
  calls.clear(); CoffeeMachine m;
  stubDelayHook=[](unsigned long ms){ t+=ms; };                      // time passes while the start-up waits
  t=1000; m.begin();
  stubDelayHook=nullptr;
  auto at=[](const char*name){ int found=-1; for(size_t i=0;i<calls.size();i++) if(calls[i]==name){ CHECK(found<0); found=(int)i; } return found; };
  const int welcome=at("welcome(0,0,0,0)"), cleared=at("endwelcome(0,0,0,0)"), icons=at("icons(0,0,0,0)"), show=at("backlight(255,0,0,0)");
  CHECK(welcome==0);                                                 // the welcome picture comes first
  CHECK(cleared>welcome && icons>cleared);                           // then it is cleared, then the icons are drawn
  CHECK(show>=0 && show==(int)calls.size()-1);                       // the backlight is the last thing of the start-up
  bool temperature=false, volume=false, duration=false;
  for(int i=0;i<show;i++){
    const bool isTemperature=startsWith(calls[i],"temp(")||startsWith(calls[i],"temperr("), isVolume=startsWith(calls[i],"vol("), isDuration=startsWith(calls[i],"dur(");
    if(isTemperature||isVolume||isDuration) CHECK(i>icons);          // the values come after the icons
    temperature|=isTemperature; volume|=isVolume; duration|=isDuration;
  }
  CHECK(temperature && volume && duration);                          // all drawn before the backlight
  CHECK(welcomeEnd-welcomeAt==WELCOME_MS);                           // shown WELCOME_MS, the 100 ms of the probe included
  printf("start-up: ok\n");
}
// The start / stop button (START_BUTTON): starts and stops the extraction only
static void pushButton(CoffeeMachine&m){ pins[PIN_START_BUTTON]=0; for(int i=0;i<5;i++){ t+=20; m.update(); } }      // pushed (the pin goes low)
static void releaseButton(CoffeeMachine&m){ pins[PIN_START_BUTTON]=1; for(int i=0;i<5;i++){ t+=20; m.update(); } }
static void startButton(){
  memset(EEPROM.mem,0xFF,64); SettingsStore st; st.load(); SAVE(st,preinfusion,0); SAVE(st,preinfusionWait,0); SAVE(st,flush,4);
  pins[PIN_START_BUTTON]=1;                                           // released
  // a button held at power-up does nothing
  { pins[PIN_START_BUTTON]=0; CoffeeMachine h; t=1000; h.begin(); for(int i=0;i<20;i++){ t+=20; h.update(); }
    CHECK(!h.extraction_.isActive()); pins[PIN_START_BUTTON]=1; }
  CoffeeMachine m; t=5000; m.begin(); for(int i=0;i<10;i++){ t+=20; m.update(); }
  CHECK(!m.extraction_.isActive());
  // a glitch shorter than the debounce time is not a push
  pins[PIN_START_BUTTON]=0; t+=10; m.update(); pins[PIN_START_BUTTON]=1; t+=10; m.update(); for(int i=0;i<5;i++){ t+=20; m.update(); }
  CHECK(!m.extraction_.isActive());
  // a push starts the extraction (the volume is selected), and acts once however long it is held
  pushButton(m);
  CHECK(m.extraction_.isActive() && m.extraction_.state()==EXTRACTION_RUNNING && PUMPON && pins[PIN_VALVE_RELAY]==1);
  CHECK(m.screen_.activeParameter()==PARAMETER_VOLUME);
  for(int i=0;i<30;i++){ t+=20; m.flow_.in=m.flow_.in+0.05f; m.update(); }
  CHECK(m.extraction_.state()==EXTRACTION_RUNNING);                  // held: still running
  releaseButton(m);
  CHECK(m.extraction_.state()==EXTRACTION_RUNNING);                  // the release does nothing
  // a push during the extraction ends it (the flush waits), then it starts the flush, then it stops it: as the encoder did
  pushButton(m);  CHECK(m.extraction_.state()==EXTRACTION_FLUSH_WAIT && !PUMPON);
  releaseButton(m); pushButton(m);  CHECK(m.extraction_.state()==EXTRACTION_FLUSHING && PUMPON);
  releaseButton(m); pushButton(m);  CHECK(m.extraction_.state()==EXTRACTION_FINISHED && !PUMPON && pins[PIN_VALVE_RELAY]==0);
  releaseButton(m);

  // a parameter being set is kept (saved), the volume is selected again and the extraction starts
  gotoParam(m,PARAMETER_TEMPERATURE); CHECK(m.screen_.activeParameter()==PARAMETER_TEMPERATURE);
  m.screen_.adjustActiveParameter(4);                                 // 93 -> 97: not validated
  CHECK(SettingsStore().load().temperature==DEFAULT_TEMPERATURE);
  pushButton(m);
  CHECK(m.screen_.activeParameter()==PARAMETER_VOLUME && m.extraction_.isActive());
  CHECK(SettingsStore().load().temperature==DEFAULT_TEMPERATURE+4);   // saved
  CHECK(m.heater_.setpoint_==DEFAULT_TEMPERATURE+4+TEMPERATURE_OFFSET);   // and applied to the heater
  releaseButton(m); pushButton(m); releaseButton(m); pushButton(m); releaseButton(m); pushButton(m); releaseButton(m);   // end, flush, stop flush
  CHECK(m.extraction_.state()==EXTRACTION_FINISHED);
  // another parameter (the volume of the pre-infusion) and the same
  gotoParam(m,PARAMETER_PREINFUSION); m.screen_.adjustActiveParameter(3);
  pushButton(m); CHECK(m.screen_.activeParameter()==PARAMETER_VOLUME && m.extraction_.isActive());
  CHECK(SettingsStore().load().preinfusion==3);
  releaseButton(m); pushButton(m); releaseButton(m); pushButton(m); releaseButton(m); pushButton(m); releaseButton(m);

  // MANUAL: a push starts the pump, the next one stops it
  m.handleShortPush(); CHECK(m.screen_.settings().manual);
  pushButton(m); CHECK(m.extraction_.state()==EXTRACTION_MANUAL && PUMPON);
  releaseButton(m); pushButton(m); CHECK(m.extraction_.state()==EXTRACTION_FINISHED && !PUMPON);
  releaseButton(m); m.handleShortPush(); CHECK(!m.screen_.settings().manual);

  // The long push in AUTO: the extraction starts at the push (AUTO), and at LONG_PUSH_MS, the button still held, it becomes a MANUAL one:
  // the pump never stops, the letter is M, the next push stops it, then AUTO is selected again. Not in the journal, the mode never saved.
  {
    Journal jl; JournalEntry je; const bool had=jl.read(0,je); const uint8_t lastSeq=had?je.sequence:0;
    pins[PIN_START_BUTTON]=0;
    bool pumpAlways=true;
    unsigned long held=0;
    while(held<LONG_PUSH_MS-100){ t+=20; held+=20; m.flow_.in=m.flow_.in+0.1f; m.update(); if(held>=100) pumpAlways=pumpAlways&&PUMPON; }
    CHECK(m.extraction_.isActive() && !m.extraction_.isManual() && !m.screen_.settings().manual);   // still AUTO before the long push
    while(held<LONG_PUSH_MS+200){ t+=20; held+=20; m.flow_.in=m.flow_.in+0.1f; m.update(); pumpAlways=pumpAlways&&PUMPON; }
    CHECK(m.extraction_.state()==EXTRACTION_MANUAL && m.extraction_.isManual() && m.screen_.settings().manual);   // MANUAL, button still held
    CHECK(pumpAlways);
    pins[PIN_START_BUTTON]=1; for(int i=0;i<20;i++){ t+=20; m.flow_.in=m.flow_.in+0.1f; m.update(); }
    CHECK(m.extraction_.state()==EXTRACTION_MANUAL && PUMPON);                // the release does nothing
    pushButton(m); CHECK(m.extraction_.state()==EXTRACTION_FINISHED && !PUMPON);   // the next push stops it
    releaseButton(m);
    CHECK(!m.screen_.settings().manual);                                       // AUTO again
    for(int i=0;i<200;i++){ t+=20; m.update(); }                               // the settings would be saved by now
    CHECK(!SettingsStore().load().manual);                                     // MANUAL was never saved
    CHECK(jl.read(0,je)==had && (!had || je.sequence==lastSeq));               // not in the journal
  }
  // a short push in AUTO stays AUTO (held less than LONG_PUSH_MS)
  pushButton(m); CHECK(m.extraction_.isActive() && !m.extraction_.isManual());
  releaseButton(m); for(int i=0;i<(int)(LONG_PUSH_MS/20)+10;i++){ t+=20; m.flow_.in=m.flow_.in+0.1f; m.update(); }
  CHECK(!m.extraction_.isManual() && !m.screen_.settings().manual);
  pushButton(m); releaseButton(m); pushButton(m); releaseButton(m); pushButton(m); releaseButton(m);   // end, flush, stop flush
  CHECK(!m.extraction_.isActive());
  // in MANUAL (chosen with the encoder) a long push changes nothing: MANUAL stays after the extraction
  m.handleShortPush(); CHECK(m.screen_.settings().manual);
  pins[PIN_START_BUTTON]=0; for(int i=0;i<(int)(LONG_PUSH_MS/20)+20;i++){ t+=20; m.flow_.in=m.flow_.in+0.1f; m.update(); }
  CHECK(m.extraction_.state()==EXTRACTION_MANUAL);
  releaseButton(m); pushButton(m); releaseButton(m);
  CHECK(!m.extraction_.isActive() && m.screen_.settings().manual);
  m.handleShortPush(); CHECK(!m.screen_.settings().manual);

  // the weight that was being entered for CAL is validated (the loss is corrected), then the extraction starts
  { memset(EEPROM.mem,0xFF,64); SettingsStore s2; s2.load(); SAVE(s2,preinfusion,0); SAVE(s2,preinfusionWait,0); SAVE(s2,flush,0);
    CoffeeMachine c; t=1000; c.begin(); pins[PIN_START_BUTTON]=1;
    runAuto(c); float net=c.extraction_.netVolumeMl(); CHECK(c.screen_.calibrationAvailable_);
    gotoParam(c,PARAMETER_CALIBRATION); c.screen_.adjustActiveParameter(-(c.screen_.calibrationValue_-30));   // the cup holds 30 g
    pushButton(c);
    const float expect=DEFAULT_LOSS_ML+CALIBRATION_GAIN*((net-30)-DEFAULT_LOSS_ML);
    CHECK(c.screen_.settings().loss>expect-0.01f && c.screen_.settings().loss<expect+0.01f);
    CHECK(savedLoss()>expect-0.01f && savedLoss()<expect+0.01f);
    CHECK(c.screen_.activeParameter()==PARAMETER_VOLUME && c.extraction_.isActive() && !c.screen_.calibrationAvailable_);
    releaseButton(c); }

  // a no-water error: the button does nothing (the short push of the encoder clears it)
  { memset(EEPROM.mem,0xFF,64); SettingsStore s3; s3.load();
    CoffeeMachine w; t=1000; w.begin(); pins[PIN_START_BUTTON]=1; for(int i=0;i<5;i++){ t+=20; w.update(); }
    pushButton(w); CHECK(w.extraction_.isActive());                   // no water counted by the (mock) flow sensor
    releaseButton(w);
    for(int i=0;i<80;i++){ t+=100; w.update(); }
    CHECK(w.extraction_.hasNoWaterError() && !PUMPON);
    pushButton(w); CHECK(w.extraction_.hasNoWaterError() && !PUMPON && !w.extraction_.isActive());
    releaseButton(w); w.handleShortPush(); CHECK(!w.extraction_.hasNoWaterError());
    pushButton(w); CHECK(w.extraction_.isActive());                   // cleared: it starts again
    releaseButton(w); }

  // standby: the first push only wakes the machine
  { memset(EEPROM.mem,0xFF,64); SettingsStore s4; s4.load();
    CoffeeMachine b; t=1000; b.begin(); pins[PIN_START_BUTTON]=1; for(int i=0;i<5;i++){ t+=20; b.update(); }
    t+=STANDBY_MINUTES*60000UL+1000; b.update(); CHECK(b.standby_);
    pushButton(b); CHECK(!b.standby_ && !b.extraction_.isActive());
    releaseButton(b); pushButton(b); CHECK(b.extraction_.isActive());   // the next one starts
    releaseButton(b); }
  printf("start button: ok\n");
}
// A cold boiler (the heater at full power, the relay on all the time): the pump and the valve work as usual, whatever the temperature
static void coldBoiler(){
  for(double probe: {30.0, 60.0, 74.0, 76.0, 93.0}){
    memset(EEPROM.mem,0xFF,64); SettingsStore st; st.load(); SAVE(st,preinfusion,0); SAVE(st,preinfusionWait,0); SAVE(st,flush,0);
    probeAt(probe); CoffeeMachine m; t=1000; m.begin(); for(int i=0;i<60;i++){ t+=100; m.update(); }
    const bool full = probe < DEFAULT_TEMPERATURE+TEMPERATURE_OFFSET-PID_WORKING_RANGE;
    if(full) CHECK(m.heater_.isOn());                                   // full power under (setpoint - 20) on the probe
    m.handleStartButton();
    bool pumpAlways=true, heaterSeen=false;
    for(int i=0;i<50;i++){ t+=100; m.flow_.in=m.flow_.in+0.05f; m.update(); pumpAlways=pumpAlways&&PUMPON&&pins[PIN_VALVE_RELAY]==1; heaterSeen=heaterSeen||m.heater_.isOn(); }
    CHECK(m.extraction_.state()==EXTRACTION_RUNNING && pumpAlways);     // the pump and the valve follow the extraction at any temperature
    if(full) CHECK(heaterSeen);
    m.extraction_.stop(EXTRACTION_FINISHED);
  }
  printf("cold boiler: ok\n");
}
// The steaming mode (long push of the encoder): the heater alone at SteamingTemp, no extraction, the temperature alone on the screen
static std::string lastSteam(){ std::string r; for(auto&c:calls) if(c.rfind("steam(",0)==0) r=c; return r; }
static void steaming(){
  memset(EEPROM.mem,0xFF,64);
  CHECK(SettingsStore().load().steamingTemperature==DEFAULT_STEAMING_TEMPERATURE && DEFAULT_STEAMING_TEMPERATURE==135);
  pins[PIN_START_BUTTON]=1;
  CoffeeMachine m; t=1000; m.begin(); for(int i=0;i<10;i++){ t+=20; m.update(); }
  CHECK(!m.steaming_ && m.heater_.setpoint_==DEFAULT_TEMPERATURE+TEMPERATURE_OFFSET);
  // a long push enters it: the screen is cleared, the heater regulates SteamingTemp, BrewingTemp stays
  calls.clear(); m.handlePush(PUSH_LONG); t+=100; m.update();
  CHECK(m.steaming_ && m.screen_.steaming() && has("clearscreen(0,0,0,0)") && has("steamicon(0,0,0,0)") && !lastSteam().empty());   // the thermometer stays
  CHECK(m.heater_.setpoint_==135+TEMPERATURE_OFFSET && m.screen_.settings().temperature==DEFAULT_TEMPERATURE);
  // no extraction: the start button does nothing (held or not), the pushes of the encoder neither, the pump and the valve stay off
  pushButton(m); CHECK(!m.extraction_.isActive() && !PUMPON && pins[PIN_VALVE_RELAY]==0); releaseButton(m);
  m.handlePush(PUSH_SHORT); m.handlePush(PUSH_DOUBLE); t+=100; m.update();
  CHECK(!m.screen_.settings().manual && m.screen_.activeParameter()==PARAMETER_TEMPERATURE && !m.extraction_.isActive());
  // the flow is not read: water that "flows" starts nothing, and no volume is drawn
  calls.clear(); for(int i=0;i<20;i++){ t+=100; m.flow_.in=m.flow_.in+0.5f; m.update(); }
  CHECK(!m.extraction_.isActive() && !has("vol(0,1,0,1)") && !PUMPON);
  for(auto&c:calls) CHECK(c.rfind("vol(",0)!=0 && c.rfind("dur(",0)!=0 && c.rfind("drop(",0)!=0 && c.rfind("temp(",0)!=0);
  // the encoder sets SteamingTemp: shown at once, saved, applied to the heater; BrewingTemp is not touched
  calls.clear(); { const int ops=EEPROM.blockOps; m.adjustActiveParameter(5); t+=100; m.update();
  CHECK(lastSteam()=="steam(140,0,0,0)" && m.heater_.setpoint_==140+TEMPERATURE_OFFSET && EEPROM.blockOps==ops);   // applied at once, not written yet
  m.adjustActiveParameter(1); step(m,100); m.adjustActiveParameter(-1); step(m,100); m.adjustActiveParameter(1); step(m,100); CHECK(EEPROM.blockOps==ops);   // a series of detents
  m.adjustActiveParameter(-1); settle(m); CHECK(EEPROM.blockOps==ops+1); for(int i=0;i<50;i++) step(m,20); CHECK(EEPROM.blockOps==ops+1); }   // one write after the delay
  CHECK(SettingsStore().load().steamingTemperature==140 && SettingsStore().load().temperature==DEFAULT_TEMPERATURE);
  CHECK(m.heater_.setpoint_==140+TEMPERATURE_OFFSET);
  // after STEAM_SETPOINT_SHOW_MS the measured temperature is shown again
  m.adjustActiveParameter(1); m.adjustActiveParameter(-1); step(m,100);   // (the setpoint is shown now)
  calls.clear(); for(int i=0;i<(int)(STEAM_SETPOINT_SHOW_MS/100)+15;i++){ t+=100; m.update(); }
  CHECK(!lastSteam().empty() && lastSteam()!="steam(140,0,0,0)");
  // a long push leaves it: the brewing again (BrewingTemp), the real screen in full, the volume selected
  calls.clear(); m.handlePush(PUSH_LONG); t+=100; m.update();
  CHECK(!m.steaming_ && !m.screen_.steaming() && has("clearscreen(0,0,0,0)") && m.screen_.activeParameter()==PARAMETER_VOLUME);
  { bool temp=false; for(auto&c:calls) temp|=c.rfind("temp(",0)==0||c.rfind("temperr(",0)==0;
    CHECK(has("icons(0,0,0,0)") && has("vol(40,1,0,1)") && has("dur(0,0,0,1)") && temp); }
  CHECK(m.heater_.setpoint_==DEFAULT_TEMPERATURE+TEMPERATURE_OFFSET);
  // an extraction works again, and a long push during it does nothing
  pushButton(m); CHECK(m.extraction_.isActive() && PUMPON); releaseButton(m);
  m.handlePush(PUSH_LONG); t+=100; m.update(); CHECK(!m.steaming_ && m.extraction_.isActive());
  pushButton(m); releaseButton(m);
  // SteamingTemp is remembered at the next start
  { CoffeeMachine m2; t=1000; m2.begin(); m2.handlePush(PUSH_LONG); CHECK(m2.heater_.setpoint_==140+TEMPERATURE_OFFSET); }
  printf("steaming: ok\n");
}
// The journal of the last extractions: how each AUTO one ended and the volumes it counted (see Journal.h)
static void journal(){
  memset(EEPROM.mem,0xFF,sizeof(EEPROM.mem)); SettingsStore st; st.load(); SAVE(st,preinfusion,0); SAVE(st,preinfusionWait,0); SAVE(st,flush,0);
  Journal j; JournalEntry e;
  CoffeeMachine m; t=1000; m.begin();
  CHECK(!j.read(0,e));                                                 // nothing yet
  // 1. AUTO to the target: written when the water after the stop is counted (not before), once
  m.handleStartButton(); unsigned long t0=t;
  for(int i=0;i<600 && m.extraction_.isActive();i++){ t+=100; m.flow_.in=(t-t0)*0.003f; m.flow_.out=0; m.update(); }
  CHECK(!j.read(0,e));                                                 // the pump just stopped: the water that comes after is being counted
  for(int i=0;i<40;i++){ t+=100; m.update(); }
  CHECK(j.read(0,e) && e.reason==END_TARGET && e.phase==EXTRACTION_RUNNING && e.sequence==0);
  CHECK(e.target==DEFAULT_VOLUME && e.preinfusion==0 && e.wait==0 && e.duration>=(DEFAULT_VOLUME+DEFAULT_LOSS_ML)/3-2 && e.duration<=(DEFAULT_VOLUME+DEFAULT_LOSS_ML)/3+2 && e.outTenths==0 && e.loss4==(uint8_t)(DEFAULT_LOSS_ML*4));
  CHECK(e.cup>=DEFAULT_VOLUME-1 && e.cup<=DEFAULT_VOLUME+2 && e.delay100>0 && e.tempStart>=85 && e.tempStart<=90 && e.tempEnd>=85 && e.tempEnd<=90);
  { const float cup=(e.inTenths-e.outTenths)/10.0f-e.loss4/4.0f; CHECK(cup>e.cup-1.0f && cup<e.cup+1.0f); }   // the volumes add up: cup = In - Out - loss
  CHECK(!j.read(1,e));
  for(int i=0;i<50;i++){ t+=100; m.update(); } CHECK(!j.read(1,e));    // written once
  // the curve of the flow In and Out of that extraction: in the curve ring, with the sequence of its entry; it adds up to the volumes
  {
    uint8_t cb[CURVE_BYTES]; uint16_t cn=0; unsigned sumIn=0, sumOut=0; CHECK(j.read(0,e));
    CHECK(j.readCurve(e.sequence,cb,cn) && e.sequence==0 && cn>=(unsigned)(e.duration*1000/CURVE_SAMPLE_MS) && cn<=(unsigned)(e.duration*1000/CURVE_SAMPLE_MS)+10);
    for(unsigned i=0;i<cn;i++){ sumIn+=cb[2*i]; sumOut+=cb[2*i+1]; }
    CHECK(sumIn>=e.inTenths-10 && sumIn<=e.inTenths+10 && sumOut==0);
    CHECK(cb[0]>0 && cb[2*(cn/2)]>0);                                  // the water flows from the start to the middle
    CHECK(!j.readCurve(1,cb,cn));                                       // one curve so far
  }
  // 2. stopped by the start button, in the main phase
  m.handleStartButton(); t0=t;
  for(int i=0;i<15;i++){ t+=100; m.flow_.in=(t-t0)*0.003f; m.update(); }
  m.handleStartButton(); for(int i=0;i<40;i++){ t+=100; m.update(); }
  CHECK(j.read(0,e) && e.reason==END_USER && e.phase==EXTRACTION_RUNNING && e.sequence==1 && e.cup<DEFAULT_VOLUME-20);
  // 3. no water: the pump runs, nothing counted
  m.handleStartButton();
  for(int i=0;i<80;i++){ t+=100; m.update(); }
  CHECK(m.extraction_.hasNoWaterError() && j.read(0,e) && e.reason==END_NO_WATER && e.phase==EXTRACTION_RUNNING && e.sequence==2 && e.inTenths==0);
  m.handleShortPush();                                                 // the error is cleared (the short push of the encoder)
  // 4. MANUAL: not in the journal (only the AUTO extractions are kept)
  m.handleShortPush(); CHECK(m.screen_.settings().manual);
  m.handleStartButton(); t0=t;
  for(int i=0;i<30;i++){ t+=100; m.flow_.in=(t-t0)*0.003f; m.flow_.out=(t-t0)*0.001f; m.update(); }
  m.handleStartButton(); for(int i=0;i<60;i++){ t+=100; m.update(); }
  CHECK(j.read(0,e) && e.reason==END_NO_WATER && !j.read(3,e));
  m.handleShortPush(); CHECK(!m.screen_.settings().manual);            // AUTO again
  // 5. the maximum duration: the water trickles, the target is never reached
  m.handleStartButton(); t0=t;
  for(int i=0;i<1400 && m.extraction_.state()!=EXTRACTION_ERROR;i++){ t+=100; m.flow_.in=m.flow_.in+0.05f; m.update(); }
  CHECK(m.extraction_.state()==EXTRACTION_ERROR && j.read(0,e) && e.reason==END_MAX_TIME && e.sequence==3 && e.duration>=DURATION_MAX && e.duration<=DURATION_MAX+2);
  // the order and the count: the last one first, four in all
  CHECK(j.read(1,e) && e.reason==END_NO_WATER && j.read(3,e) && e.reason==END_TARGET && !j.read(4,e));
  // 6. with a pre-infusion (10 ml, 3 s of wait): the extractions that end in the pre-infusion or in its wait are kept too, with the phase
  m.screen_.settings_.preinfusion=10; m.screen_.settings_.preinfusionWait=3;
  m.handleStartButton(); for(int i=0;i<10;i++){ t+=100; m.flow_.in=m.flow_.in+0.3f; m.update(); }
  CHECK(m.extraction_.state()==EXTRACTION_PREINFUSION);
  m.handleStartButton(); for(int i=0;i<40;i++){ t+=100; m.update(); }   // stopped in the pre-infusion
  CHECK(j.read(0,e) && e.reason==END_USER && e.phase==EXTRACTION_PREINFUSION && e.preinfusion==10 && e.wait==3 && e.sequence==4 && j.read(4,e) && !j.read(5,e));
  m.handleStartButton();
  for(int i=0;i<100 && m.extraction_.state()!=EXTRACTION_PREINFUSION_WAIT;i++){ t+=100; m.flow_.in=m.flow_.in+0.3f; m.update(); }
  CHECK(m.extraction_.state()==EXTRACTION_PREINFUSION_WAIT);
  for(int i=0;i<10;i++){ t+=100; m.update(); }
  m.handleStartButton(); for(int i=0;i<40;i++){ t+=100; m.update(); }   // stopped in the wait
  CHECK(j.read(0,e) && e.reason==END_USER && e.phase==EXTRACTION_PREINFUSION_WAIT && e.sequence==5 && j.read(5,e) && !j.read(6,e));
  m.handleStartButton();
  for(int i=0;i<200 && m.extraction_.state()!=EXTRACTION_RUNNING;i++){ t+=100; if(m.extraction_.state()==EXTRACTION_PREINFUSION) m.flow_.in=m.flow_.in+0.3f; m.update(); }
  CHECK(m.extraction_.state()==EXTRACTION_RUNNING);
  for(int i=0;i<10;i++){ t+=100; m.flow_.in=m.flow_.in+0.3f; m.update(); }
  m.handleStartButton(); for(int i=0;i<40;i++){ t+=100; m.update(); }   // stopped in the main phase
  CHECK(j.read(0,e) && e.reason==END_USER && e.phase==EXTRACTION_RUNNING && e.preinfusion==10 && e.sequence==6 && j.read(6,e) && !j.read(7,e));
  // 7. the ring keeps the last JOURNAL_COUNT (50): more extractions (quick ones) than it holds, the oldest ones go
  for(int n=0;n<110;n++){
    m.handleStartButton(); for(int i=0;i<15;i++){ t+=100; m.update(); }
    m.handleStartButton(); for(int i=0;i<30;i++){ t+=100; m.update(); }   // stopped in the pre-infusion
  }
  CHECK(j.read(0,e) && e.reason==END_USER && e.phase==EXTRACTION_PREINFUSION && e.sequence==(uint8_t)(6+110) && j.read(JOURNAL_COUNT-1,e) && !j.read(JOURNAL_COUNT,e));
  CHECK(j.read(JOURNAL_COUNT-1,e) && e.phase==EXTRACTION_PREINFUSION);   // the first ones are gone: the oldest is one of the last quick ones
  printf("journal: ok\n");
}
// The data flash is written outside of the extraction: a block operation takes the loop for some milliseconds and the stop of the pump
// must never wait for it (CoffeeMachine::flashQuiet(): not while the pump runs or a timed phase counts; the wait for the push that
// starts the flush is allowed). What is due waits, and is written afterwards: the journal, the stop delay, the settings, the curve.
static void flashOutsideExtraction(){
  memset(EEPROM.mem,0xFF,sizeof(EEPROM.mem)); SettingsStore st; st.load(); SAVE(st,preinfusion,0); SAVE(st,preinfusionWait,0); SAVE(st,flush,4);
  CoffeeMachine m; t=1000; m.begin(); Journal j; JournalEntry e;
  int bad=0;   // block operations seen while the pump runs or a phase is timed
  auto pass=[&](unsigned long dt){ t+=dt; const int before=EEPROM.blockOps; m.update();
    if(EEPROM.blockOps!=before && m.extraction_.isActive() && m.extraction_.state()!=EXTRACTION_FLUSH_WAIT) bad++;
    if(EEPROM.blockOps>before+1) bad++; };   // and one block operation per pass at most
  m.screen_.adjustActiveParameter(2);
  int ops=EEPROM.blockOps; m.handleStartButton();
  CHECK(EEPROM.blockOps==ops+1 && m.extraction_.isActive() && !PUMPON==false);       // what was set is written before the pump starts
  ops=EEPROM.blockOps; unsigned long t0=t; int last=ops;
  for(int i=0;i<600 && m.extraction_.state()!=EXTRACTION_FLUSH_WAIT;i++){
    last=EEPROM.blockOps;
    m.flow_.in=(t+100-t0)*0.003f; pass(100);
    if(i==50){ m.screen_.settings_.temperature+=1; m.saveSettings(); }                // a change during the extraction (not the user's way, but it can be pending)
  }
  CHECK(m.extraction_.state()==EXTRACTION_FLUSH_WAIT && bad==0 && last==ops);   // nothing written from the start of the pump to its stop (the pass that ends it may write)
  for(int i=0;i<40;i++) pass(100);                                                      // waiting for the push of the flush: the pump is off
  CHECK(bad==0 && j.read(0,e) && e.sequence==0 && EEPROM.blockOps>ops);                // the entry, the stop delay, the settings, the curve
  { int16_t temp; EEPROM.get(1,temp); CHECK(temp==DEFAULT_TEMPERATURE+1); }
  m.handleStartButton(); CHECK(m.extraction_.state()==EXTRACTION_FLUSHING && PUMPON);
  ops=EEPROM.blockOps;
  m.screen_.settings_.temperature+=1; m.saveSettings();
  for(int i=0;i<30 && m.extraction_.isActive();i++) pass(100);                          // the flush runs: nothing is written, even long after the delay
  CHECK(m.extraction_.isActive()==false || bad==0);
  for(int i=0;i<60;i++) pass(100);
  CHECK(bad==0 && EEPROM.blockOps==ops+1 && !m.extraction_.isActive());                 // the change waited for the end of the flush: one block operation
  { int16_t temp; EEPROM.get(1,temp); CHECK(temp==DEFAULT_TEMPERATURE+2); }
  // a pump that is stopped by the user at once: the settings changed before the push are written first (before the pump), never after
  ops=EEPROM.blockOps; m.screen_.adjustActiveParameter(1); m.handleStartButton(); CHECK(EEPROM.blockOps==ops+1);
  for(int i=0;i<10;i++){ m.flow_.in=m.flow_.in+0.3f; pass(100); }
  m.handleStartButton(); for(int i=0;i<100;i++) pass(100);
  CHECK(bad==0);
  printf("flash outside the extraction: ok\n");
}
// The delay of the stop before the target is learned, saved and given back at the next start
static void stopDelay(){
  memset(EEPROM.mem,0xFF,64); SettingsStore st; st.load(); SAVE(st,preinfusion,0); SAVE(st,preinfusionWait,0); SAVE(st,flush,0); SAVE(st,volume,5);
  CoffeeMachine m; t=1000; m.begin();
  CHECK(m.extraction_.stopDelaySeconds()==STOP_DELAY_DEFAULT_S);        // nothing learned yet: the default
  m.handleStartButton(); unsigned long t0=t; bool pumped=false; float tail=0;
  for(int i=0;i<1500;i++){                                               // 5 ml/s of In; 3 ml still come after the pump stops
    t+=10;
    if(m.extraction_.isActive()){ m.flow_.in=(t-t0)*0.005f; m.flow_.rate=300; pumped=true; tail=3.0f; }
    else if(pumped){ m.flow_.rate=0; float d=tail<0.03f?tail:0.03f; m.flow_.in+=d; tail-=d; }
    m.update();
  }
  const float learned=m.extraction_.stopDelaySeconds();
  CHECK(learned>STOP_DELAY_DEFAULT_S && learned<STOP_DELAY_DEFAULT_S+0.3f);   // 3 ml at 5 ml/s = 0.6 s: it moved toward it
  { CoffeeMachine m2; t+=1000; m2.begin(); CHECK(m2.extraction_.stopDelaySeconds()==learned); }   // saved, given back at the start
  printf("stop delay: ok\n");
}
int main(){ SPI.hook=answer; probeAt(93.0); scenario(0); scenario(3); autoSave(); noWater(); temperatureError(); startup(); manualMode(); manualTime(); calibration(); startButton(); journal(); coldBoiler(); steaming(); flashOutsideExtraction(); stopDelay(); standby(); printf(fails?"%d FAIL\n":"ALL OK\n",fails); return fails!=0; }
