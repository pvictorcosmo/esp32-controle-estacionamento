#pragma once
#include "Arduino.h"
#define WL_CONNECTED 3
#define WIFI_STA 1
struct IP{String toString(){return String("10.10.0.2");}};
struct WiFiC{int st=0; void mode(int){} void setAutoReconnect(bool){} void begin(const char*,const char*,int){} int status(){return st;} IP localIP(){return IP();} int RSSI(){return -58;}};
extern WiFiC WiFi;
