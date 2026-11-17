#include <cstring>
#include "src/Config.h"
#include "src/machine/ExtractionController.h"
#include <cstdio>
#include "TestSupport.h"
int main(){
  FlowSensor f; ExtractionController e(4,16,f); e.begin(); f.begin(); t=0;
  e.start(0,10,3,2); ExtractionState last=(ExtractionState)99; unsigned long fl=0;
  for(t=0;t<60000;t+=10){
    if(e.state()==EXTRACTION_FLUSH_WAIT && t>=40000 && fl==0){ fl=t; e.startFlush(t);} 
    e.update(t,36);
    if(e.state()!=last || t%2000==0){ printf("t=%5lu st=%d pump=%d valve=%d cup=%3d ml dur=%ds in=%.1f net=%.1f\n",t,e.state(),pins[4],pins[16],e.volumeMl(),e.durationSeconds(),f.getVolumeIn(),f.getNetVolume()); last=e.state(); }
    if(e.state()==EXTRACTION_FINISHED) break;
  }
  CHECK(e.state()==EXTRACTION_FINISHED && e.volumeMl()==36 && e.durationSeconds()>=26 && e.durationSeconds()<=28 && pins[4]==0 && pins[16]==0);
  printf(fails?"%d FAIL\n":"ALL OK\n",fails); return fails!=0;
}
