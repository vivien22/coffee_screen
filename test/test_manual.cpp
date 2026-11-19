// MANUAL mode of the extraction controller: pump and valve until stop(), volume In - Out
#include "EEPROM.h"
#include "src/Config.h"
#include "src/coffee/ExtractionController.h"
#include <cstdio>
#include "TestSupport.h"
#define PUMP pins[4]
#define VALVE pins[16]
int main(){
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=1000; e.startManual(1000,0);
    CHECK(e.state()==EXTRACTION_MANUAL && e.isActive() && PUMP==1 && VALVE==1);
    CHECK(e.preinfusionMlLeft()==0 && e.phaseSecondsLeft()==0);
    // volume = In - Out, nothing removed (a normal extraction would show 0 until 30 ml)
    f.in=10; f.out=1; for(t=1250;t<=2000;t+=250) e.update(t,5); CHECK(e.volumeMl()==9 && e.durationSeconds()==1);
    // the target volume (5) does not stop it, and the main extraction does not start a flush
    f.in=60; f.out=0; for(;t<=9000;t+=250){ f.in+=0.8f; e.update(t,5); }
    CHECK(e.state()==EXTRACTION_MANUAL && PUMP==1 && VALVE==1 && e.durationSeconds()==8);
    float net=f.in-f.out;
    e.stop(EXTRACTION_FINISHED);
    CHECK(e.state()==EXTRACTION_FINISHED && !e.isActive() && PUMP==0 && VALVE==0);
    CHECK(e.volumeMl()==(uint16_t)(net+0.5f) && e.durationSeconds()==8);
    // finish() has no effect on it, a new normal extraction is back to the normal volume
    e.start(t,0,0,0); CHECK(e.state()==EXTRACTION_RUNNING); f.in=50; f.out=0; t+=250; e.update(t,500); CHECK(e.volumeMl()==20); }
  // finish() does not apply to MANUAL
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.startManual(0,0); e.finish(t);
    CHECK(e.state()==EXTRACTION_MANUAL && PUMP==1); }
  // no-flow safety is active
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.startManual(0,0);
    for(t=250;t<=NO_FLOW_TIMEOUT_MS;t+=250) e.update(t,40);
    CHECK(e.state()==EXTRACTION_ERROR_NO_WATER && PUMP==0 && VALVE==0);
    e.startManual(t,0); CHECK(e.state()==EXTRACTION_ERROR_NO_WATER); }
  // maximum duration
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.startManual(0,0);
    for(t=0;t<(DURATION_MAX+2)*1000UL;t+=1000){ f.in=t*0.003f; e.update(t,40); if(!e.isActive()) break; }
    CHECK(e.state()==EXTRACTION_ERROR && PUMP==0); }
  // ignored while an extraction is in progress
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,0,0,0); e.startManual(t,0); CHECK(e.state()==EXTRACTION_RUNNING); }
  // ---- MANUAL time: the pump stops by itself, the countdown is for the screen
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=1000; e.startManual(1000,5);
    CHECK(e.state()==EXTRACTION_MANUAL && PUMP==1 && e.phaseSecondsLeft()==5);          // 5 at once
    f.in=0; for(t=1250;t<=3000;t+=250){ f.in+=1; e.update(t,5); }
    CHECK(e.state()==EXTRACTION_MANUAL && e.phaseSecondsLeft()==3);                    // 2 s elapsed of 5
    for(;t<=5900;t+=250){ f.in+=1; e.update(t,5); }
    CHECK(e.state()==EXTRACTION_MANUAL && e.phaseSecondsLeft()==1 && PUMP==1);
    for(;t<=6100;t+=250){ f.in+=1; e.update(t,5); }
    CHECK(e.state()==EXTRACTION_FINISHED && PUMP==0 && VALVE==0 && e.phaseSecondsLeft()==0);
    CHECK(e.durationSeconds()==5 || e.durationSeconds()==6);                           // elapsed time at the end
    CHECK(e.volumeMl()>=18); }
  // a push before the end stops it too; 0 s keeps the old behaviour (no countdown)
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.startManual(0,30);
    for(t=250;t<=2000;t+=250){ f.in+=1; e.update(t,5); } e.stop(EXTRACTION_FINISHED); CHECK(e.state()==EXTRACTION_FINISHED && PUMP==0); }
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.startManual(0,0);
    for(t=250;t<=20000;t+=250){ f.in+=1; e.update(t,5); } CHECK(e.state()==EXTRACTION_MANUAL && e.phaseSecondsLeft()==0); }
  printf(fails?"%d FAIL\n":"ALL OK\n",fails); return fails!=0;
}
