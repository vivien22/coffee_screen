#include <vector>
#include <string>
#include <cstring>
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
int main(){
  const float LOSS=DEFAULT_LOSS_ML;   // 30
  // ---- cup volume estimate = In - Out - 30, never below 0
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,0,0,0);
    f.in=20; f.out=0; t=300; e.update(t,40); CHECK(e.volumeMl()==0);               // priming: nothing in the cup yet
    f.in=50; f.out=0; t=600; e.update(t,40); CHECK(e.volumeMl()==20);              // 50 - 30
    f.in=55; f.out=10; t=900; e.update(t,40); CHECK(e.volumeMl()==15);             // Out (OPV) is subtracted
    f.in=10; f.out=40; t=1200; e.update(t,40); CHECK(e.volumeMl()==0);             // negative net -> 0
    CHECK(e.isActive());
    f.in=69; f.out=0; t=1500; e.update(t,40); CHECK(e.volumeMl()==39 && e.isActive());   // not yet
    f.in=70; t=1800; e.update(t,40); CHECK(e.volumeMl()==40 && !e.isActive() && e.state()==EXTRACTION_FINISHED); // stops at target + 30
    CHECK(PUMP==0 && pins[16]==0);
    CHECK(e.volumeMl()==40); }                                                       // final value = target
  // ---- feedforward uses the inflow rate
  { FlowSensor f; ExtractionController e(4,16,f); e.begin(); t=0; e.start(0,0,0,0); f.rate=120; CHECK(e.inflowMlPerSec()>1.99f && e.inflowMlPerSec()<2.01f);
    e.stop(EXTRACTION_FINISHED); CHECK(e.inflowMlPerSec()==0.0f); }
  printf(fails?"%d FAIL\n":"ALL OK\n",fails); return fails!=0;
}
