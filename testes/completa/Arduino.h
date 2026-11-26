#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdarg>
#include <string>
#include <deque>
#include <map>
#include <functional>
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define CHANGE 3
#define IRAM_ATTR
#define PROGMEM
extern unsigned long T; extern unsigned long uoff; extern int pinv[40]; extern int servoAng[40]; extern int toneF;
inline unsigned long millis(){return T;}
inline unsigned long micros(){return T*1000+uoff;}
inline void pinMode(int,int){}
inline int digitalRead(int p){return pinv[p];}
void digitalWrite(int p,int v);
inline void delayMicroseconds(int){}
inline void delay(int){}
inline int digitalPinToInterrupt(int p){return p;}
typedef void(*isrfn)();
extern isrfn isrs[40];
inline void attachInterrupt(int p,isrfn f,int){isrs[p]=f;}
typedef int gpio_num_t;
inline int gpio_get_level(gpio_num_t p){return pinv[p];}
inline void tone(int,unsigned f){toneF=f;}
inline void noTone(int){toneF=0;}
template<class A,class B,class C> A constrain(A x,B a,C b){return x<a?a:(x>b?b:x);}
class String{ public: std::string s; String(){} String(const char*c):s(c){} String(const std::string&x):s(x){}
 String(float f,int d){char b[32];snprintf(b,32,"%.*f",d,f);s=b;}
 void reserve(int){} const char* c_str()const{return s.c_str();}
 String& operator+=(const char*c){s+=c;return *this;} String& operator+=(char c){s+=c;return *this;}
 String& operator+=(const String&o){s+=o.s;return *this;}
 String& operator+=(int v){s+=std::to_string(v);return *this;} String& operator+=(unsigned v){s+=std::to_string(v);return *this;}
 String& operator+=(long v){s+=std::to_string(v);return *this;} String& operator+=(unsigned long v){s+=std::to_string(v);return *this;}
 bool operator==(const char*c)const{return s==c;} };
struct SerialC{ std::deque<char> in; std::string out;
 void begin(long){} int available(){return in.size();} int read(){char c=in.front();in.pop_front();return c;}
 void print(const char*s){out+=s;} void print(char c){out+=c;} void print(const String&s){out+=s.s;}
 void println(const char*s=""){out+=s;out+="\n";} void println(const String&s){out+=s.s+"\n";}
 void printf(const char*f,...){char b[256];va_list a;va_start(a,f);vsnprintf(b,256,f,a);va_end(a);out+=b;}};
extern SerialC Serial;
