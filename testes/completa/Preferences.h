#pragma once
#include "Arduino.h"
extern std::map<std::string,long> nvsMap;
struct Preferences{ void begin(const char*,bool){} void end(){}
 int getInt(const char*k,int d){return nvsMap.count(k)?nvsMap[k]:d;} unsigned getUInt(const char*k,unsigned d){return nvsMap.count(k)?nvsMap[k]:d;}
 void putInt(const char*k,int v){nvsMap[k]=v;} void putUInt(const char*k,unsigned v){nvsMap[k]=v;} };
