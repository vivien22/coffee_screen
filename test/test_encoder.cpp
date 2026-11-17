// CJMCU111: pushes and rotations decoded by the interrupt routine (pins driven by the test)
#define private public
#include "src/drivers/CJMCU111.h"
#undef private
#include <cstdio>
#include "TestSupport.h"
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
  // ---- the long push is reported while the button is held
  { CJMCU111 e3(2,3,800,350); byte l,r; PushEvent ev;
    t+=1000; set(0,1); t+=850; e3.update(l,r,ev); CHECK(ev==PUSH_LONG);
    t+=10; set(1,1); t+=1; e3.update(l,r,ev); CHECK(ev==PUSH_NONE); }
  printf(fails?"%d FAIL\n":"ALL OK\n",fails); return fails!=0;
}
