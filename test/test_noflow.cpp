// No-flow safety: the pump runs but the water does not enter the boiler
#include "EEPROM.h"
#include "src/Config.h"
#include "src/machine/ExtractionController.h"
#include <cstdio>
static unsigned long t=0; unsigned long millis(){return t;} void delay(unsigned long){}
static int pins[32]; void pinMode(uint8_t,uint8_t){} void digitalWrite(uint8_t p,uint8_t v){pins[p]=v;}
int digitalRead(uint8_t){return 0;} void noInterrupts(){} void interrupts(){} SerialT Serial; EEPROMClass EEPROM;
static int fails=0;
#define CHECK(c) do{ if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;} }while(0)
#define PUMP pins[4]
#define VALVE pins[16]
int main(){
  // ---- RUNNING, empty tank: no water at all, error after NO_FLOW_TIMEOUT_MS
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,0,0,0);
    for(t=250;t<NO_FLOW_TIMEOUT_MS;t+=250) e.update(t,40);
    CHECK(e.state()==EXTRACTION_RUNNING && PUMP==1);
    t=NO_FLOW_TIMEOUT_MS; e.update(t,40);
    CHECK(e.state()==EXTRACTION_ERROR_NO_WATER && e.hasNoWaterError() && !e.isActive() && PUMP==0 && VALVE==0);
    // no new extraction until clearError()
    e.start(t,0,0,0); CHECK(e.state()==EXTRACTION_ERROR_NO_WATER && PUMP==0);
    e.clearError(); CHECK(e.state()==EXTRACTION_IDLE && !e.hasNoWaterError());
    e.start(t,0,0,0); CHECK(e.state()==EXTRACTION_RUNNING && PUMP==1); }
  // ---- normal flow (3 ml/s): never an error
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,0,0,0);
    for(t=250;t<12000;t+=250){ f.in=t*0.003f; e.update(t,200); }
    CHECK(e.state()==EXTRACTION_RUNNING); }
  // ---- the flow stops during the extraction (tank emptied)
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,0,0,0);
    for(t=250;t<4000;t+=250){ f.in=t*0.003f; e.update(t,200); }
    float stuck=f.in; for(;t<4000+NO_FLOW_TIMEOUT_MS-250;t+=250){ f.in=stuck; e.update(t,200); }
    CHECK(e.state()==EXTRACTION_RUNNING);
    t+=250; e.update(t,200); CHECK(e.state()==EXTRACTION_ERROR_NO_WATER && PUMP==0); }
  // ---- choked puck: the water comes back through the OPV (In - Out stops) but In still counts: no error
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,0,0,0);
    for(t=250;t<=3*NO_FLOW_TIMEOUT_MS;t+=250){ f.in=t*0.003f; f.out=f.in; e.update(t,40); }
    CHECK(e.state()==EXTRACTION_RUNNING && PUMP==1); }
  // ---- In sensor disconnected: Out alone does not hide the error
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,0,0,0);
    for(t=250;t<=NO_FLOW_TIMEOUT_MS;t+=250){ f.out=t*0.003f; e.update(t,40); }
    CHECK(e.state()==EXTRACTION_ERROR_NO_WATER); }
  // ---- PREINFUSION without flow too (before the 30 s timeout)
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,10,3,0);
    for(t=250;t<=NO_FLOW_TIMEOUT_MS;t+=250) e.update(t,40);
    CHECK(e.state()==EXTRACTION_ERROR_NO_WATER && PUMP==0); }
  // ---- the pause of PREINFUSION_WAIT (pump off) is not a no-flow: the reference restarts with RUNNING
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,10,8,0);
    for(t=250;t<4000;t+=250){ f.in=t*0.003f*2; e.update(t,200); if(e.state()!=EXTRACTION_PREINFUSION) break; }
    CHECK(e.state()==EXTRACTION_PREINFUSION_WAIT);
    for(;t<4000+8000+500;t+=250) e.update(t,200);
    CHECK(e.state()==EXTRACTION_RUNNING && PUMP==1);
    t+=250; f.in+=1; e.update(t,200); CHECK(e.state()==EXTRACTION_RUNNING); }
  // ---- the flush is not watched
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,0,0,10);
    f.in=100; t=250; e.update(t,500); e.finish(t); CHECK(e.state()==EXTRACTION_FLUSH_WAIT);
    e.startFlush(t); for(t+=250;t<250+9000;t+=250) e.update(t,500); CHECK(e.state()==EXTRACTION_FLUSHING); }
  printf(fails?"%d FAIL\n":"ALL OK\n",fails); return fails!=0;
}
