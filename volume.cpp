#include "volume.h"

int getVolume(int time){
  if(time>=VOLUME_TABLE_SIZE){
    time = VOLUME_TABLE_SIZE - 1;
  }
  return volumeTable[time];
}
