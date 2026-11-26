#pragma once
#include <cstdint>
#include <cstdio>
#include <cctype>
#include <string>
#include <deque>
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 1
extern unsigned long T; extern int pinv[40]; extern int servoAng[40];
inline unsigned long millis(){return T;}
inline void pinMode(int,int){}
inline int digitalRead(int p){return pinv[p];}
inline void digitalWrite(int p,int v){pinv[p]=v;}
template<class A,class B,class C> A constrain(A x,B a,C b){return x<a?a:(x>b?b:x);}
struct SerialC{ std::deque<char> in; std::string out;
 void begin(long){} int available(){return in.size();} int read(){char c=in.front();in.pop_front();return c;}
 void print(const char*s){out+=s;} void print(char c){out+=c;} void println(const char*s=""){out+=s;out+="\n";}};
extern SerialC Serial;
