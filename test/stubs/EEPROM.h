#pragma once
#include "Arduino.h"
#include <string.h>
struct EEPROMClass { uint8_t mem[64];
 uint8_t read(int a){return mem[a];} void update(int a,uint8_t v){mem[a]=v;}
 template<class T> T& put(int a,const T& t){memcpy(mem+a,&t,sizeof(T)); static T x; x=t; return x;}
 template<class T> T& get(int a,T& t){memcpy(&t,mem+a,sizeof(T)); return t;} };
extern EEPROMClass EEPROM;
