#pragma once
#define AUTOMATIC 1
#define MANUAL 0
#define DIRECT 0
#define P_ON_E 1
class PID { public: PID(double*,double*,double*,double,double,double,int,int){}
 void SetMode(int){} void SetOutputLimits(double,double){} void SetSampleTime(int){} bool Compute(){return true;} };
