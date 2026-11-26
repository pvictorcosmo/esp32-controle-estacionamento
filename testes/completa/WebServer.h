#pragma once
#include "Arduino.h"
#define HTTP_GET 1
#define HTTP_POST 2
struct WebServer{ std::map<std::string,std::function<void()>> r; std::string lastBody, argv; int lastCode=0;
 WebServer(int){} void on(const char*p,int,std::function<void()>f){r[p]=f;}
 void onNotFound(std::function<void()>){} void begin(){} void handleClient(){}
 void sendHeader(const char*,const char*){} void send(int c,const char*,const String&b){lastCode=c;lastBody=b.s;}
 void send(int c,const char*t,const char*b){send(c,t,String(b));}
 String arg(const char*){return String(argv);} };
