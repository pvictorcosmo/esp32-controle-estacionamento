#include "Arduino.h"
unsigned long T=0; int pinv[40]; int servoAng[40]; SerialC Serial;
#include "../../01-referencia/sketch.ino"
void run(unsigned long ms){for(unsigned long i=0;i<ms;i++){loop();T++;}}
void press(int pin,unsigned long hold=100){ // with bounce
 for(int k=0;k<5;k++){pinv[pin]=k%2?HIGH:LOW;run(2);} pinv[pin]=LOW;run(hold);
 for(int k=0;k<5;k++){pinv[pin]=k%2?LOW:HIGH;run(2);} pinv[pin]=HIGH;run(60);}
int fails=0;
#define CHECK(c,msg) do{bool ok=(c);printf("%s %s\n",ok?"PASS":"FAIL",msg);if(!ok)fails++;}while(0)
int main(){ for(int i=0;i<40;i++)pinv[i]=HIGH; setup(); run(10);
 CHECK(veiculos==0&&pinv[21]==HIGH&&pinv[22]==LOW&&servoAng[25]==0&&servoAng[26]==0,"TF01 inicio");
 press(18); CHECK(veiculos==1&&servoAng[25]==90,"TF02 entrada abre"); run(2500); CHECK(servoAng[25]==0,"TF02 fecha apos 2,5s");
 press(19); CHECK(veiculos==0&&servoAng[26]==90,"TF03 saida abre"); run(2500); CHECK(servoAng[26]==0,"TF03 fecha");
 Serial.in.push_back('t'); Serial.in.push_back('\n'); run(5); CHECK(veiculos==29,"TF04 T=29");
 press(18); CHECK(veiculos==30&&pinv[22]==HIGH&&pinv[21]==LOW,"TF05 lotado LED vermelho"); run(2500);
 press(18); CHECK(veiculos==30&&servoAng[25]==0,"TF06 acesso negado");
 press(19); CHECK(veiculos==29&&pinv[21]==HIGH&&pinv[22]==LOW,"TF07 saida com lotacao"); run(2500);
 Serial.in.push_back('R'); run(5); press(19); CHECK(veiculos==0&&servoAng[26]==0,"TF08 nao fica negativo");
 press(18,5000); CHECK(veiculos==1,"TF09 segurar = 1 entrada"); run(2500);
 press(18); unsigned long t0=T; press(19); CHECK(servoAng[25]==90&&servoAng[26]==90,"TF10 ambas abertas juntas");
 run(2500-(T-t0)); CHECK(servoAng[25]==0&&servoAng[26]==90,"TF10 fecham independentes");
 run(200); CHECK(servoAng[26]==0,"TF10 saida fecha depois");
 
 return fails; }
