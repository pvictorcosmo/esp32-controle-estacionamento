#pragma once
#include "Arduino.h"
struct ESP32PWM{static void allocateTimer(int){}};
struct Servo{int p=-1; Servo(){} void setPeriodHertz(int){} void attach(int pin,int,int){p=pin;} void write(int a){servoAng[p]=a;}};
