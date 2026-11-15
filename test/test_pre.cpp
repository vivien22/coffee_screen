#include <vector>
#include <string>
#include <cstring>
#include "EEPROM.h"
#include "src/config.h"
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
void drawTemperature(double v,TextSize s){} void drawVolume(uint16_t v,TextSize s,PhaseTag g,bool r){L("vol",v,s,g,r);}
void drawDuration(uint16_t v,TextSize s,PhaseTag g,bool r){L("dur",v,s,g,r);}
void clearTemperature(){} void clearVolume(){L("clearVol");} void clearDuration(){L("clearDur");} void drawHeaterIndicator(bool){} void drawPhaseTag(PhaseTag g,bool v){L("tag",0,g,v);} void drawDrop(bool v,bool m){L("drop",m,v);} void drawTemperatureError(){L("temperr");}
static int fails=0;
#define CHECK(c) do{ if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;} }while(0)
#define PUMP pins[4]
#define VALVE pins[16]
static bool has(const char*s){ for(auto&c:calls) if(c==s) return true; return false; }
int main(){
  // ---- pre-infusion by volume: 10 ml, then 2 s of wait, then RUNNING
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,10,2,0);
    CHECK(e.state()==EXTRACTION_PREINFUSION && PUMP==1 && VALVE==1 && e.preinfusionMlLeft()==10);
    f.in=2.5f; t=250; e.update(t,40); CHECK(e.state()==EXTRACTION_PREINFUSION && e.preinfusionMlLeft()==8 && e.phaseSecondsLeft()==0);   // ceil(7.5)
    f.in=9.0f; t=500; e.update(t,40); CHECK(e.preinfusionMlLeft()==1);
    f.out=0.5f; t=750; e.update(t,40); CHECK(e.state()==EXTRACTION_PREINFUSION && e.preinfusionMlLeft()==2);   // Out is subtracted
    f.in=10.5f; t=1000; e.update(t,40); CHECK(e.state()==EXTRACTION_PREINFUSION_WAIT && PUMP==0 && VALVE==1);   // 10.0 net
    CHECK(e.preinfusionMlLeft()==0 && e.phaseSecondsLeft()==2);
    t=2100; e.update(t,40); CHECK(e.state()==EXTRACTION_PREINFUSION_WAIT && e.phaseSecondsLeft()==1);
    t=3100; e.update(t,40); CHECK(e.state()==EXTRACTION_RUNNING && PUMP==1);
    CHECK(e.volumeMl()==0);                                                    // 10 ml entered: still nothing in the cup
  }
  // ---- the target volume does not end the pre-infusion, it is checked in RUNNING only
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,50,0,0);
    f.in=45; t=300; e.update(t,5); CHECK(e.state()==EXTRACTION_PREINFUSION && e.isActive());     // cup estimate 15 >= target 5: still pre-infusing
    f.in=50; t=600; e.update(t,5);                                                              // pre-infusion done, RUNNING, then the target (5) is already reached
    CHECK(e.state()==EXTRACTION_FINISHED && e.volumeMl()==20); }
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,50,0,0);
    f.in=50; t=300; e.update(t,100); CHECK(e.state()==EXTRACTION_RUNNING);                      // 50 - 30 = 20 < 100
    f.in=130; t=600; e.update(t,100); CHECK(e.state()==EXTRACTION_FINISHED && e.volumeMl()==100); }
  // ---- safety: very low flow (0.5 ml/s: not a no-flow error), the pre-infusion ends after PREINFUSION_TIMEOUT_MS
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,20,3,0);
    for(t=250;t<PREINFUSION_TIMEOUT_MS;t+=250){ f.in=t*0.0005f; e.update(t,40); }
    CHECK(e.state()==EXTRACTION_PREINFUSION);
    t=PREINFUSION_TIMEOUT_MS; f.in=t*0.0005f; e.update(t,40); CHECK(e.state()==EXTRACTION_PREINFUSION_WAIT && PUMP==0); }
  // ---- pre-infusion volume 0: neither pre-infusion nor wait
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,0,5,0); CHECK(e.state()==EXTRACTION_RUNNING && PUMP==1); }
  // ---- settings
  memset(EEPROM.mem,0xFF,64); SettingsStore st; Settings s=st.load();
  CHECK(s.preinfusion==DEFAULT_PREINFUSION_ML && s.preinfusion==10 && EEPROM.mem[0]==0xA8);
  st.savePreinfusion(50); s=st.load(); CHECK(s.preinfusion==50);
  EEPROM.mem[5]=200; s=st.load(); CHECK(s.preinfusion==PREINFUSION_MAX_ML);
  EEPROM.mem[6]=77; s=st.load(); CHECK(s.preinfusionWait==PREINFUSION_WAIT_MAX_SECONDS);
  // ---- screen: the pre-infusion volume is edited in the volume field, with the PRE tag
  { ScreenController sc; Settings in={93,40,10,5,5}; sc.begin(in); sc.update(0);
    sc.selectNextParameter(); CHECK(sc.activeParameter()==PARAMETER_PREINFUSION);
    calls.clear(); sc.update(1); CHECK(has("vol(10,1,1,1)")); CHECK(!has("dur(10,1,1,1)"));
    CHECK(sc.adjustActiveParameter(3)); calls.clear(); sc.update(2); CHECK(calls.size()==1 && calls[0]=="vol(13,1,1,0)");   // number only
    CHECK(sc.adjustActiveParameter(100)); CHECK(sc.settings().preinfusion==50 && !sc.adjustActiveParameter(1));
    CHECK(sc.settings().preinfusionWait==5 && sc.settings().volume==40);
    calls.clear(); sc.confirmSave(10); CHECK(calls.size()==1 && calls[0]=="clearVol(0,0,0,0)");
    calls.clear(); sc.update(700); CHECK(has("vol(50,1,1,1)"));          // redrawn after the blink
    sc.selectNextParameter(); CHECK(sc.activeParameter()==PARAMETER_PREINFUSION_WAIT);
    calls.clear(); sc.update(701); CHECK(has("dur(5,1,2,1)")); CHECK(has("vol(40,0,0,1)"));   // wait in the duration field; the volume field is back to the target, small
    CHECK(sc.adjustActiveParameter(4)); CHECK(sc.settings().preinfusionWait==9); CHECK(sc.adjustActiveParameter(100)); CHECK(sc.settings().preinfusionWait==10);
  }
  // ---- screen during an extraction
  { ScreenController sc; Settings in={93,40,10,5,5}; sc.begin(in); sc.update(0); sc.showExtractionStarted(); sc.update(1);
    sc.showPreinfusion(10,0); calls.clear(); sc.update(2);
    CHECK(has("vol(10,1,1,1)")); CHECK(calls.size()==1);                 // countdown 10 with the PRE tag, large digits (volume selected)
    sc.showPreinfusion(9,0); calls.clear(); sc.update(3); CHECK(calls.size()==1 && calls[0]=="vol(9,1,1,0)");   // number only
    sc.showPreinfusion(9,1); calls.clear(); sc.update(4); CHECK(calls.size()==1 && calls[0]=="dur(1,0,0,0)");   // elapsed time runs, no tag
    sc.showPreinfusionWait(0,5); calls.clear(); sc.update(5);
    CHECK(has("vol(0,1,0,1)") && has("dur(5,0,2,1)"));                    // PRE tag gone (whole volume field), WAIT tag appears
    sc.showPreinfusionWait(0,4); calls.clear(); sc.update(6); CHECK(calls.size()==1 && calls[0]=="dur(4,0,2,0)");
    sc.showExtractionProgress(12,9); calls.clear(); sc.update(7); CHECK(has("vol(12,1,0,0)") && has("dur(9,0,0,1)"));
  }
  printf(fails?"%d FAIL\n":"ALL OK\n",fails); return fails!=0;
}
