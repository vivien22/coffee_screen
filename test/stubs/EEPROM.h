#pragma once
#include "Arduino.h"
#include <string.h>
// EEPROM of the tests: like the real one, update() and put() write only the bytes that changed;
// "writes" counts the bytes really written (wear).
struct EEPROMClass { uint8_t mem[64]; int writes = 0;
 uint8_t read(int a){return mem[a];}
 void update(int a,uint8_t v){ if(mem[a]!=v){ mem[a]=v; writes++; } }
 template<class T> T& put(int a,const T& t){ const uint8_t* p=(const uint8_t*)&t; for(unsigned i=0;i<sizeof(T);i++) update(a+i,p[i]); static T x; x=t; return x; }
 template<class T> T& get(int a,T& t){memcpy(&t,mem+a,sizeof(T)); return t;} };
extern EEPROMClass EEPROM;
