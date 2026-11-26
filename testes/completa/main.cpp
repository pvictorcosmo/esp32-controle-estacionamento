#include "Arduino.h"
#include "WiFi.h"
unsigned long T=0,uoff=0; int pinv[40]; int servoAng[40]; int toneF=0; SerialC Serial; WiFiC WiFi;
isrfn isrs[40]; std::map<std::string,long> nvsMap;
float distCm[40]; int lastTrig[40];
#include "../../02-versao-completa/sketch.ino"
// trig 13 -> echo 14 ; trig 27 -> echo 34
void digitalWrite(int p,int v){
  int prev=pinv[p]; pinv[p]=v;
  if((p==13||p==27)&&prev==HIGH&&v==LOW&&isrs[p==13?14:34]){ int e=p==13?14:34; float d=distCm[e];
    if(d<=400){ pinv[e]=HIGH; isrs[e](); uoff+=(unsigned long)(d*58); pinv[e]=LOW; isrs[e](); } }
}
void run(unsigned long ms){for(unsigned long i=0;i<ms;i++){loop();T++;}}
void press(int pin,unsigned long hold=100){
 for(int k=0;k<5;k++){pinv[pin]=k%2?HIGH:LOW;run(2);} pinv[pin]=LOW;run(hold);
 for(int k=0;k<5;k++){pinv[pin]=k%2?LOW:HIGH;run(2);} pinv[pin]=HIGH;run(60);}
int fails=0;
#define CHECK(c,msg) do{bool ok=(c);printf("%s %s\n",ok?"PASS":"FAIL",msg);if(!ok)fails++;}while(0)
std::string lcdL(int r){return std::string(lcd.scr[r]);}
int main(){ for(int i=0;i<40;i++)pinv[i]=HIGH; pinv[14]=pinv[34]=LOW; distCm[14]=distCm[34]=300;
 setup(); run(1600);
 CHECK(veiculos==0&&pinv[32]==HIGH&&pinv[33]==LOW&&servoAng[25]==0,"TF01 inicio");
 printf("   LCD: [%s] [%s]\n",lcdL(0).c_str(),lcdL(1).c_str());
 press(18); run(400); CHECK(veiculos==1&&servoAng[25]==90&&cancelaEntrada.estado==EstadoCancela::ABERTA,"TF02 entrada abre (suave)");
 printf("   LCD: [%s] [%s]\n",lcdL(0).c_str(),lcdL(1).c_str());
 run(2500); CHECK(servoAng[25]==0&&cancelaEntrada.estado==EstadoCancela::FECHADA,"TF02 fecha");
 press(19); run(3000); CHECK(veiculos==0&&servoAng[26]==0&&cancelaSaida.aberturas==1,"TF03 saida");
 Serial.in.push_back('T'); run(5); CHECK(veiculos==29,"TF04 T");
 press(18); CHECK(veiculos==30&&pinv[33]==HIGH&&pinv[32]==LOW,"TF05 lotado"); run(3000);
 press(18); CHECK(veiculos==30&&stats.negados==1&&cancelaEntrada.estado==EstadoCancela::FECHADA,"TF06 negado");
 run(2100); printf("   LCD: [%s] [%s]\n",lcdL(0).c_str(),lcdL(1).c_str());
 press(19); CHECK(veiculos==29&&pinv[32]==HIGH,"TF07 saida lotado"); run(3000);
 Serial.in.push_back('R'); run(5); press(19); CHECK(veiculos==0,"TF08 nao negativo");
 press(18,5000); CHECK(veiculos==1,"TF09 segurar"); run(3000);
 press(18); unsigned long t0=T; press(19); CHECK(cancelaEntrada.estaAberta()&&cancelaSaida.estaAberta(),"TF10 independentes");
 run(2750-(T-t0)); CHECK(!cancelaEntrada.estaAberta()&&cancelaSaida.estaAberta(),"TF10 fecham em tempos proprios"); run(800);
 // sensores
 int v0=veiculos; distCm[14]=30; run(400); CHECK(veiculos==v0+1&&cancelaEntrada.estaAberta(),"SENSOR chegada conta e abre");
 run(4000); CHECK(veiculos==v0+1&&cancelaEntrada.estado==EstadoCancela::ABERTA,"SEGURANCA nao fecha com veiculo embaixo");
 distCm[14]=60; run(400); CHECK(cancelaEntrada.estado==EstadoCancela::ABERTA,"HISTERESE 60cm ainda presente");
 distCm[14]=300; run(800); CHECK(cancelaEntrada.estado==EstadoCancela::FECHADA&&veiculos==v0+1,"veiculo saiu -> fecha sem contar de novo");
 press(18); run(2600); CHECK(cancelaEntrada.estado==EstadoCancela::FECHANDO,"fechando...");
 int v1=veiculos; distCm[14]=20; run(400); CHECK(cancelaEntrada.estaAberta()&&veiculos==v1+1,"REABERTURA ao detectar durante fechamento");
 distCm[14]=300; run(4000);
 distCm[34]=25; run(400); CHECK(veiculos==v1,"SENSOR saida decrementa"); distCm[34]=300; run(4000);
 distCm[14]=45; run(30); distCm[14]=300; run(300); CHECK(veiculos==v1,"ruido de 1 leitura ignorado");
 // web
 WiFi.st=WL_CONNECTED; run(600); CHECK(servidorAtivo,"servidor web iniciado");
 servidor.argv="teste"; servidor.r["/api/comando"](); CHECK(veiculos==29&&servidor.lastCode==200,"API comando teste");
 servidor.argv="xyz"; servidor.r["/api/comando"](); CHECK(servidor.lastCode==400,"API acao invalida 400");
 servidor.r["/api/estado"](); FILE*f=fopen("estado.json","w"); fputs(servidor.lastBody.c_str(),f); fclose(f);
 servidor.r["/"](); f=fopen("pagina.html","w"); fputs(servidor.lastBody.c_str(),f); fclose(f);
 run(2500); CHECK(nvsMap["veic"]==29,"NVS salvou contador");
 printf("   LCD: [%s] [%s]\n",lcdL(0).c_str(),lcdL(1).c_str());
 f=fopen("serial.txt","w"); fputs(Serial.out.c_str(),f); fclose(f);
 printf("fails=%d\n",fails); return fails; }
