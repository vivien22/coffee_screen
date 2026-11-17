// Temperature sensor: PT100 resistance -> degrees C (polynomial) against the exact Callendar-Van Dusen formula
#include <cmath>
#include "src/config.h"
#include "src/heating/temperature_sensor.h"
#include <SPI.h>
#include <cstdio>
SPIClass SPI;
static unsigned long t=0; unsigned long millis(){return t;} void delay(unsigned long){}
void pinMode(uint8_t,uint8_t){} void digitalWrite(uint8_t,uint8_t){} int digitalRead(uint8_t){return 0;}
void noInterrupts(){} void interrupts(){} SerialT Serial;
static int fails=0;
#define CHECK(c) do{ if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;} }while(0)
// The MAX31865 answers the RTD register (2 bytes after the address): 15 bits of ratio, then the fault bit
static uint16_t rtdRegister=0; static int byteIndex=0;
static uint8_t answer(uint8_t v){ if(v==0x01){ byteIndex=0; return 0; } if(v!=0) { byteIndex=-1; return 0; }
  if(byteIndex<0) return 0; return byteIndex++==0 ? (uint8_t)(rtdRegister>>8) : (uint8_t)rtdRegister; }
static double exactCelsius(double r){ const double A=3.9083e-3,B=-5.775e-7; return (-A+sqrt(A*A-4*B*(1-r/RNOMINAL)))/(2*B); }
int main(){
  SPI.hook=answer;
  double worst=0;
  for(int celsius=1;celsius<=199;celsius++){
    const double A=3.9083e-3,B=-5.775e-7; double r=RNOMINAL*(1+A*celsius+B*celsius*celsius);
    uint16_t code=(uint16_t)(r/RREF*32768.0+0.5); rtdRegister=(uint16_t)(code<<1);
    TemperatureSensor s(6); t=0; s.begin();                       // first sample: not filtered
    CHECK(s.isValid());
    double exact=exactCelsius(code*RREF/32768.0);                // the exact formula on the same reading
    double e=fabs(s.celsius()-exact); if(e>worst) worst=e;
  }
  printf("worst error against the exact formula: %.4f C\n",worst);
  CHECK(worst<0.03);
  // out of range and sensor fault: not valid
  { rtdRegister=0; TemperatureSensor s(6); s.begin(); CHECK(!s.isValid()); }                      // 0 ohm
  { rtdRegister=(uint16_t)(32767u<<1); TemperatureSensor s(6); s.begin(); CHECK(!s.isValid()); }   // 428 ohm: far above 199 C
  { rtdRegister=(uint16_t)((8000u<<1)|1); TemperatureSensor s(6); s.begin(); CHECK(!s.isValid()); } // fault bit
  // Stray readings: fewer than SENSOR_FAULT_TOLERANCE faulty readings in a row keep the sensor valid (with the last
  // good temperature), SENSOR_FAULT_TOLERANCE of them make it invalid, and one good reading brings it back
  {
    const double A=3.9083e-3,B=-5.775e-7; const double r=RNOMINAL*(1+A*93+B*93*93);
    const uint16_t good=(uint16_t)(((uint16_t)(r/RREF*32768.0+0.5))<<1);
    const uint16_t faultBit=(uint16_t)((8000u<<1)|1), tooHot=(uint16_t)(32767u<<1);
    for(int kind=0;kind<2;kind++){                                 // 0: fault bit, 1: implausible value
      const uint16_t bad = kind==0 ? faultBit : tooHot;
      rtdRegister=good; TemperatureSensor s(6); t=0; s.begin(); CHECK(s.isValid());
      const double before=s.celsius();
      for(int n=1;n<=SENSOR_FAULT_TOLERANCE;n++){
        rtdRegister=bad; t+=SENSOR_PERIOD_MS; s.update(t);
        CHECK(s.isValid()==(n<SENSOR_FAULT_TOLERANCE));
        if(n<SENSOR_FAULT_TOLERANCE) CHECK(s.celsius()==before);   // the last good temperature is kept
      }
      rtdRegister=good; t+=SENSOR_PERIOD_MS; s.update(t); CHECK(s.isValid());      // one good reading: valid again
      for(int n=1;n<SENSOR_FAULT_TOLERANCE;n++){                   // and the count started again from zero
        rtdRegister=bad; t+=SENSOR_PERIOD_MS; s.update(t); CHECK(s.isValid());
      }
    }
  }
  printf(fails?"%d FAIL\n":"ALL OK\n",fails); return fails!=0;
}
