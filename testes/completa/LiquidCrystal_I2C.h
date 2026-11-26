#pragma once
#include "Arduino.h"
struct LiquidCrystal_I2C{ char scr[2][17]; int r=0,c=0; LiquidCrystal_I2C(int,int,int){memset(scr,' ',sizeof scr);scr[0][16]=scr[1][16]=0;}
 void init(){} void backlight(){} void createChar(int,uint8_t*){} void setCursor(int cc,int rr){c=cc;r=rr;}
 void print(const char*s){for(;*s&&c<16;s++)scr[r][c++]=(*s==1?'@':*s==2?'_':*s==3?'|':*s);} };
