// CJMCU111: pushes and rotations decoded by the interrupt routine (pins driven by the test)
#define private public
#include "src/lib/CJMCU111.h"
#undef private
#include <cstdio>
static unsigned long t=0; unsigned long millis(){return t;} void delay(unsigned long){}
static int pins[32]; void pinMode(uint8_t,uint8_t){} void digitalWrite(uint8_t p,uint8_t v){pins[p]=v;}
int digitalRead(uint8_t p){return pins[p];} void noInterrupts(){} void interrupts(){} SerialT Serial;
static int fails=0;
#define CHECK(c) do{ if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;} }while(0)
// Set Ga / Gb, 10 ms later, and run the interrupt routine
static void set(int ga,int gb){ t+=10; pins[2]=ga; pins[3]=gb; CJMCU111::isrGx(); }
int main(){
  CJMCU111 enc(2,3,800,350); pins[2]=1; pins[3]=1; enc.begin();
  byte l,r,p;
  // push: Ga LOW then HIGH, Gb does not move
  set(0,1); set(1,1); l=r=p=0; enc.read(l,r,p); CHECK(p==1 && l==0 && r==0);
  // left turn: Ga, both, Gb, rest
  set(0,1); set(0,0); set(1,0); set(1,1); l=r=p=0; enc.read(l,r,p); CHECK(l==1 && r==0 && p==0);
  // right turn
  set(1,0); set(0,0); set(0,1); set(1,1); l=r=p=0; enc.read(l,r,p); CHECK(r==1 && l==0 && p==0);
  // left turn started (both LOW) then cancelled: neither a rotation nor a push
  set(0,1); set(0,0); set(0,1); set(1,1); l=r=p=0; enc.read(l,r,p); CHECK(p==0 && l==0 && r==0);
  // and a real push still works after that
  set(0,1); set(1,1); l=r=p=0; enc.read(l,r,p); CHECK(p==1);
  // ---- very long push enabled (3 s): long push on release (0.8 .. 3 s), very long push while held
  { CJMCU111 e2(2,3,800,350,3000); byte l,r; PushEvent ev;
    auto upd=[&](){ return e2.update(l,r,ev); };
    t+=1000; set(0,1);                                  // press
    t+=900; upd(); CHECK(ev==PUSH_NONE);                // held 0.9 s: nothing yet
    t+=10; set(1,1); t+=1; upd(); CHECK(ev==PUSH_LONG); // released: long push
    t+=1000; set(0,1);
    t+=2900; upd(); CHECK(ev==PUSH_NONE);               // 2.9 s
    t+=200; upd(); CHECK(ev==PUSH_VERY_LONG);           // 3.1 s: very long push, button still held
    t+=500; upd(); CHECK(ev==PUSH_NONE);
    t+=10; set(1,1); t+=1; upd(); CHECK(ev==PUSH_NONE); // its release is ignored
    t+=1000; upd(); CHECK(ev==PUSH_NONE);
    t+=1000; set(0,1); t+=100; set(1,1);                // short push
    t+=1; upd(); CHECK(ev==PUSH_NONE);                  // waits for a possible second push
    t+=400; upd(); CHECK(ev==PUSH_SHORT);
  }
  // ---- very long push disabled (default): the long push is reported while held, as before
  { CJMCU111 e3(2,3,800,350); byte l,r; PushEvent ev;
    t+=1000; set(0,1); t+=850; e3.update(l,r,ev); CHECK(ev==PUSH_LONG);
    t+=10; set(1,1); t+=1; e3.update(l,r,ev); CHECK(ev==PUSH_NONE); }
  printf(fails?"%d FAIL\n":"ALL OK\n",fails); return fails!=0;
}
