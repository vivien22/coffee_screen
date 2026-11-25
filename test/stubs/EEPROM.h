#pragma once
#include "Arduino.h"
#include <string.h>
// EEPROM of the tests: like the real one, update() and put() write only the bytes that changed; "writes" counts the bytes really
// written (wear). On the R4 the EEPROM is the data flash in blocks of 1 KB and every call that changes something erases and programs a
// whole block: "blockOps" counts those operations, as the board does (update() that changes = 1, put() = 1 per block with a change,
// and settings/Flash.cpp = 1 per block with a change). A test reads the counters before and after what it looks at.
struct EEPROMClass { uint8_t mem[8192] = {0}; int writes = 0; int blockOps = 0;   // 8 KB like the data flash of the R4
 static const int BLOCK = 1024;   // FLASH_BLOCK_SIZE of the R4
 uint16_t length(){return sizeof(mem);}
 uint8_t read(int a){return mem[a];}
 void update(int a,uint8_t v){ if(mem[a]!=v){ mem[a]=v; writes++; blockOps++; } }
 // one operation per block that holds a change (what put() and settings/Flash.cpp do on the board)
 void putBytes(int a,const uint8_t* p,unsigned n){
   for(int b=a/BLOCK; b<=(int)(a+n-1)/BLOCK && n; b++){
     bool changed=false;
     for(int i=a>b*BLOCK?a:b*BLOCK; i<(int)(a+n) && i<(b+1)*BLOCK; i++) if(mem[i]!=p[i-a]){ mem[i]=p[i-a]; writes++; changed=true; }
     if(changed) blockOps++;
   } }
 template<class T> T& put(int a,const T& t){ putBytes(a,(const uint8_t*)&t,sizeof(T)); static T x; x=t; return x; }
 template<class T> T& get(int a,T& t){memcpy(&t,mem+a,sizeof(T)); return t;} };
extern EEPROMClass EEPROM;
