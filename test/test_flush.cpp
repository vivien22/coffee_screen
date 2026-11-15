#include <vector>
#include <string>
#include <cstring>
#include "Arduino.h"
#include "EEPROM.h"
#include "src/settings/settings.h"
#include "src/machine/extraction_controller.h"
#include "src/display/screen_controller.h"
#include <cstdio>
static unsigned long t=0; unsigned long millis(){return t;} void delay(unsigned long){}
static int pins[32]; void pinMode(uint8_t,uint8_t){} void digitalWrite(uint8_t p,uint8_t v){pins[p]=v;}
int digitalRead(uint8_t){return 0;} void noInterrupts(){} void interrupts(){} SerialT Serial; EEPROMClass EEPROM;
std::vector<std::string> calls;
static void L(const char*f,double a=0,int b=0,int c=0,int d=0){char s[80];snprintf(s,80,"%s(%g,%d,%d,%d)",f,a,b,c,d);calls.push_back(s);}
void beginDisplay(){} void drawIcons(){}
void drawTemperature(double v,TextSize s){L("temp",v,s);} void drawVolume(uint16_t v,TextSize s,PhaseTag g,bool r){L("vol",v,s,g,r);}
void drawDuration(uint16_t v,TextSize s,PhaseTag g,bool r){L("dur",v,s,g,r);}
void clearTemperature(){L("clearTemp");} void clearVolume(){L("clearVol");} void clearDuration(){L("clearDur");}
void drawHeaterIndicator(bool){} void drawPhaseTag(PhaseTag g,bool v){L("tag",0,g,v);} void drawDrop(bool v){L("drop",0,v);}
static int fails=0;
#define CHECK(c) do{ if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;} }while(0)
const char* nm[]={"IDLE","PRE","WAIT","RUN","FLUSH_WAIT","FLUSHING","FIN","ERR"};
#define PUMP pins[4]
#define VALVE pins[16]
int main(){
  // ---- flush = 3 s, no pre-infusion: RUN -> (target volume) -> FLUSH_WAIT -> push -> FLUSHING -> FIN
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,0,0,3);
    CHECK(e.state()==EXTRACTION_RUNNING && PUMP==1 && VALVE==1);
    for(t=0;t<30000;t+=50){ f.vol=t/1000.0f*30; e.update(t,40); if(e.state()==EXTRACTION_FLUSH_WAIT) break; }
    CHECK(e.state()==EXTRACTION_FLUSH_WAIT); CHECK(PUMP==0 && VALVE==0); CHECK(e.isActive() && e.isFlush());
    uint16_t vol=e.volumeMl(), dur=e.durationSeconds(); CHECK(vol>=40 && vol<=45);
    CHECK(e.phaseSecondsLeft()==3);                       // shows the flush time
    unsigned long t0=t;
    for(t=t0;t<t0+60000;t+=50){ f.vol+=5; e.update(t,40); }   // waits a long time, no timeout, nothing changes
    CHECK(e.state()==EXTRACTION_FLUSH_WAIT && PUMP==0 && VALVE==0);
    CHECK(e.volumeMl()==vol && e.durationSeconds()==dur);     // extraction values frozen
    e.finish(t); CHECK(e.state()==EXTRACTION_FLUSH_WAIT);     // finish ignored in flush
    e.startFlush(t); CHECK(e.state()==EXTRACTION_FLUSHING && PUMP==1 && VALVE==1);
    unsigned long t1=t; int seen[4]={0}; 
    for(;t<t1+10000;t+=50){ f.vol+=5; e.update(t,40); if(e.state()!=EXTRACTION_FLUSHING) break; if(e.phaseSecondsLeft()<=3) seen[e.phaseSecondsLeft()]=1; }
    CHECK(seen[3]&&seen[2]&&seen[1]);                         // 3, 2, 1
    CHECK(e.state()==EXTRACTION_FINISHED && PUMP==0 && VALVE==0 && !e.isActive());
    CHECK(t-t1>=3000 && t-t1<=3100);                          // lasts 3 s
    CHECK(e.volumeMl()==vol && e.durationSeconds()==dur);     // still the extraction values
    e.startFlush(t); CHECK(e.state()==EXTRACTION_FINISHED);   // startFlush ignored
  }
  // ---- flush = 0: bypassed, RUN -> FINISHED directly
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,0,0,0);
    for(t=0;t<30000;t+=50){ f.vol=t/1000.0f*30; e.update(t,40); if(!e.isActive()) break; }
    CHECK(e.state()==EXTRACTION_FINISHED && PUMP==0 && VALVE==0); }
  // ---- user stop (finish) in RUNNING goes to FLUSH_WAIT, in PRE too; flush stopped early; stop() aborts
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,5,5,4);
    CHECK(e.state()==EXTRACTION_PREINFUSION); t=1000; e.update(t,40); e.finish(t);
    CHECK(e.state()==EXTRACTION_FLUSH_WAIT && PUMP==0 && VALVE==0);
    e.startFlush(t); t+=500; e.update(t,40); CHECK(e.state()==EXTRACTION_FLUSHING && e.phaseSecondsLeft()==4);
    uint16_t d=e.durationSeconds(); e.stop(EXTRACTION_FINISHED); CHECK(e.state()==EXTRACTION_FINISHED && PUMP==0 && VALVE==0 && e.durationSeconds()==d);
    e.start(t,0,0,4); e.update(t,40); e.finish(t); CHECK(e.state()==EXTRACTION_FLUSH_WAIT);
    e.stop(EXTRACTION_FINISHED); CHECK(e.state()==EXTRACTION_FINISHED && PUMP==0 && VALVE==0);   // double push: abort, no flush
    e.start(t,0,0,4); e.update(t,40); e.stop(EXTRACTION_FINISHED); CHECK(e.state()==EXTRACTION_FINISHED); }  // stop() = no flush
  // ---- ERROR (duration guard) does not go to flush
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,0,0,5);
    for(t=0;t<1100000;t+=1000){ f.vol=t*0.0005f; e.update(t,999); if(!e.isActive()) break; }
    CHECK(e.state()==EXTRACTION_ERROR && PUMP==0 && VALVE==0); }
  // ---- valve opens before the pump when the flush starts: checked by order in code (setValve then setPump)
  // ---- settings
  memset(EEPROM.mem,0xFF,64); SettingsStore st; Settings s=st.load();
  CHECK(s.flush==DEFAULT_FLUSH_SECONDS && s.preinfusion==10 && s.preinfusionWait==5 && EEPROM.mem[0]==0xA8);
  st.saveFlush(8); st.savePreinfusion(2); s=st.load(); CHECK(s.flush==8 && s.preinfusion==2);
  st.saveFlush(0); s=st.load(); CHECK(s.flush==0);
  // unknown magic (old layouts are no longer migrated): defaults
  memset(EEPROM.mem,0xFF,64); EEPROM.mem[0]=0xA6; int16_t tv=91,vv=36; memcpy(EEPROM.mem+1,&tv,2); memcpy(EEPROM.mem+3,&vv,2); EEPROM.mem[5]=7; EEPROM.mem[6]=2;
  s=st.load(); CHECK(s.temperature==DEFAULT_TEMPERATURE&&s.volume==DEFAULT_VOLUME&&s.preinfusion==10&&s.preinfusionWait==5&&s.flush==5&&EEPROM.mem[0]==0xA8);
  s=st.load(); CHECK(s.temperature==DEFAULT_TEMPERATURE&&s.flush==5);   // stable on the next boot
  st.saveTemperature(95); st.saveVolume(150); st.savePreinfusion(7); st.savePreinfusionWait(0); st.saveFlush(9);
  s=st.load(); CHECK(s.temperature==95&&s.volume==150&&s.preinfusion==7&&s.preinfusionWait==0&&s.flush==9);
  // clamp
  EEPROM.mem[7]=200; s=st.load(); CHECK(s.flush==10);
  // ---- screen: parameter order and flush display
  ScreenController sc; Settings in={93,40,5,5,6}; sc.begin(in);
  ActiveParameter seq[]={PARAMETER_PREINFUSION,PARAMETER_PREINFUSION_WAIT,PARAMETER_FLUSH,PARAMETER_TEMPERATURE,PARAMETER_VOLUME};
  for(int i=0;i<5;i++){ sc.selectNextParameter(); CHECK(sc.activeParameter()==seq[i]); }
  sc.selectNextParameter(); sc.selectNextParameter(); sc.selectNextParameter();   // pre, wait, flush
  CHECK(sc.activeParameter()==PARAMETER_FLUSH);
  calls.clear(); sc.update(0); bool ok=false; for(auto&c:calls) if(c=="dur(6,1,3,1)") ok=true; CHECK(ok);   // 6 s, large, FLUSH tag, tag drawn
  CHECK(sc.adjustActiveParameter(3)); CHECK(sc.settings().flush==9 && sc.settings().preinfusionWait==5);
  calls.clear(); sc.update(1); CHECK(calls.size()==1 && calls[0]=="dur(9,1,3,0)");   // number only
  CHECK(sc.adjustActiveParameter(50)); CHECK(sc.settings().flush==10); CHECK(!sc.adjustActiveParameter(1));
  CHECK(sc.adjustActiveParameter(-50)); CHECK(sc.settings().flush==0);
  calls.clear(); sc.confirmSave(10); CHECK(calls.size()==1 && calls[0]=="clearDur(0,0,0,0)");
  // flush during an extraction: tag appears once, then number-only redraws
  { ScreenController s2; Settings i2={93,40,5,5,5}; s2.begin(i2); s2.update(0);
    s2.showExtractionProgress(40,28); s2.update(1);
    s2.showFlush(40,5,false); calls.clear(); s2.update(2); bool full=false; for(auto&c:calls) if(c=="dur(5,0,3,1)") full=true; CHECK(full && calls.size()==1);
    s2.showFlush(40,5,false); calls.clear(); s2.update(3); CHECK(calls.empty());
    s2.showFlush(40,4,true); calls.clear(); s2.update(4); CHECK(calls.size()==1 && calls[0]=="dur(4,0,3,0)");
    // blinking during FLUSHING: hidden at +500 ms, shown at +1000 ms, only the tag is redrawn
    calls.clear(); s2.update(300); CHECK(calls.empty());
    s2.showFlush(40,4,true); calls.clear(); s2.update(4+599); CHECK(calls.empty());
    s2.update(4+600); CHECK(calls.size()==1 && calls[0]=="tag(0,3,0,0)");     // hidden
    s2.showFlush(40,3,true); calls.clear(); s2.update(4+700); CHECK(calls.size()==1 && calls[0]=="dur(3,0,3,0)");   // countdown: number only, tag left hidden
    calls.clear(); s2.update(4+1200); CHECK(calls.size()==1 && calls[0]=="tag(0,3,1,0)");   // shown
    calls.clear(); s2.update(4+1800); CHECK(calls.size()==1 && calls[0]=="tag(0,3,0,0)");   // hidden again
    // end of the flush while the tag is hidden: whole field redrawn without tag, blinking stops
    s2.showExtractionProgress(40,28); calls.clear(); s2.update(4+1900); CHECK(calls.size()==1 && calls[0]=="dur(28,0,0,1)");
    calls.clear(); s2.update(4+6000); CHECK(calls.empty());
  }
  printf(fails?"%d FAIL\n":"ALL OK\n",fails);
  return fails!=0;
}
