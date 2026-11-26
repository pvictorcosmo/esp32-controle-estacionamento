/*
 * ════════════════════════════════════════════════════════════════════
 *   CONTROLE AUTOMATIZADO DE ESTACIONAMENTO — ESP32 · v2 (completa)
 * ════════════════════════════════════════════════════════════════════
 *   Versão de referência + as 3 atividades do roteiro + extras:
 *
 *   Atividade 1 · Display LCD 16×2 I2C com ocupadas/livres, ícones das
 *                 cancelas e avisos temporários
 *   Atividade 2 · Sensores HC-SR04 (leitura por interrupção, sem
 *                 pulseIn bloqueante) com histerese e confirmação
 *   Atividade 3 · Painel Web via Wi-Fi: vagas, cancelas, sensores,
 *                 estatísticas, histórico e comandos remotos
 *
 *   Extras
 *   • Cancelas com movimento suave e máquina de estados
 *     (FECHADA → ABRINDO → ABERTA → FECHANDO)
 *   • Segurança: a cancela não fecha com veículo embaixo e reabre se
 *     um veículo for detectado durante o fechamento
 *   • Buzzer com sons distintos (entrada, saída, negado, lotado)
 *   • Contador e estatísticas salvos na flash (NVS / Preferences)
 *   • Histórico dos últimos eventos (serial, web)
 *   • Botões manuais do operador continuam funcionando (teclas E/S)
 *   • Tudo não bloqueante: nenhum delay() no loop
 *
 *   Pinos, tempos e credenciais → config.h  ·  Página web → pagina_web.h
 * ════════════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <Preferences.h>
#include <stdarg.h>

#include "config.h"
#include "pagina_web.h"
#include "componentes.h"

// ═════════════════════════════════════════════════════════════════════
//  Estado global
// ═════════════════════════════════════════════════════════════════════
int veiculos = 0;

struct Estatisticas {
  uint32_t entradas = 0;
  uint32_t saidas   = 0;
  uint32_t negados  = 0;
  int      pico     = 0;
} stats;

bool somAtivo        = true;
bool dadosAlterados  = false;   // há algo para gravar na NVS
bool wifiConectado   = false;
bool servidorAtivo   = false;

// ═════════════════════════════════════════════════════════════════════
//  Histórico de eventos (buffer circular)
// ═════════════════════════════════════════════════════════════════════
Evento  eventos[MAX_EVENTOS];
uint8_t eventosInicio = 0;   // índice do mais antigo
uint8_t eventosTotal  = 0;

void imprimirTimestamp(unsigned long ms) {
  char buf[16];
  snprintf(buf, sizeof(buf), "[%02lu:%02lu.%03lu] ",
           (ms / 60000UL) % 100, (ms / 1000UL) % 60, ms % 1000UL);
  Serial.print(buf);
}

// Registra no histórico e imprime no serial (formato printf)
void registrar(TipoEvento tipo, const char* formato, ...) {
  Evento& ev = eventos[(eventosInicio + eventosTotal) % MAX_EVENTOS];
  if (eventosTotal < MAX_EVENTOS) eventosTotal++;
  else eventosInicio = (eventosInicio + 1) % MAX_EVENTOS;

  ev.instante = millis();
  ev.tipo     = tipo;
  va_list args;
  va_start(args, formato);
  vsnprintf(ev.texto, sizeof(ev.texto), formato, args);
  va_end(args);

  imprimirTimestamp(ev.instante);
  Serial.print(TIPO_ICONE[tipo]);
  Serial.print(' ');
  Serial.print(TIPO_TAG[tipo]);
  Serial.print(' ');
  Serial.println(ev.texto);
}

// ═════════════════════════════════════════════════════════════════════
//  Instâncias
// ═════════════════════════════════════════════════════════════════════
Botao              btnEntrada     { PIN_BTN_ENTRADA };
Botao              btnSaida       { PIN_BTN_SAIDA };
SensorUltrassonico sensorEntrada  { "entrada", PIN_US_ENT_TRIG, PIN_US_ENT_ECHO };
SensorUltrassonico sensorSaida    { "saida",   PIN_US_SAI_TRIG, PIN_US_SAI_ECHO };
Cancela            cancelaEntrada { "entrada", PIN_SERVO_ENTRADA, &sensorEntrada };
Cancela            cancelaSaida   { "saída",   PIN_SERVO_SAIDA,   &sensorSaida };
Buzzer             buzzer         { PIN_BUZZER };
LiquidCrystal_I2C  lcd(LCD_ENDERECO, 16, 2);
WebServer          servidor(80);
Preferences        nvs;

// ═════════════════════════════════════════════════════════════════════
//  Display LCD 16×2
// ═════════════════════════════════════════════════════════════════════
const uint8_t ICONE_CARRO[8]   = { 0b00000, 0b00000, 0b01110, 0b11111, 0b11111, 0b01010, 0b00000, 0b00000 };
const uint8_t ICONE_FECHADA[8] = { 0b00000, 0b00000, 0b11111, 0b11111, 0b10000, 0b10000, 0b10000, 0b10000 };
const uint8_t ICONE_ABERTA[8]  = { 0b11000, 0b11000, 0b11000, 0b11000, 0b11000, 0b10000, 0b10000, 0b10000 };
constexpr char CH_CARRO = 1, CH_FECHADA = 2, CH_ABERTA = 3;

char          lcdLinhas[2][17] = { "", "" };   // o que já está na tela
char          lcdAviso[17]     = "";
unsigned long lcdAvisoAte      = 0;

void lcdEscrever(uint8_t linha, const char* texto) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", texto);   // completa com espaços
  if (strcmp(buf, lcdLinhas[linha]) == 0) return; // evita I2C desnecessário
  lcd.setCursor(0, linha);
  lcd.print(buf);
  strcpy(lcdLinhas[linha], buf);
}

void lcdAvisar(const char* texto) {
  strncpy(lcdAviso, texto, 16);
  lcdAviso[16] = '\0';
  lcdAvisoAte  = millis() + LCD_MENSAGEM_MS;
}

void lcdIniciar() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.createChar(CH_CARRO,   (uint8_t*)ICONE_CARRO);
  lcd.createChar(CH_FECHADA, (uint8_t*)ICONE_FECHADA);
  lcd.createChar(CH_ABERTA,  (uint8_t*)ICONE_ABERTA);
  lcdEscrever(0, " ESTACIONAMENTO");
  lcdEscrever(1, "  ESP32 \x01 v2.0");
  lcdAvisoAte = millis() + 1500;               // tela de abertura
  lcdAviso[0] = '\0';
}

void lcdAtualizar() {
  static unsigned long ultimo = 0;
  if (millis() - ultimo < 100) return;         // 10 quadros/s bastam
  ultimo = millis();
  if ((long)(lcdAvisoAte - millis()) > 0 && lcdAviso[0] == '\0') return; // splash

  char l0[17], l1[17];
  if ((long)(lcdAvisoAte - millis()) > 0) {
    snprintf(l0, sizeof(l0), "%s", lcdAviso);
  } else {
    snprintf(l0, sizeof(l0), "%c Ocupadas %2d/%d", CH_CARRO, veiculos, CAPACIDADE);
  }

  char esquerda[20];
  if (veiculos >= CAPACIDADE) snprintf(esquerda, sizeof(esquerda), "LOTADO!");
  else                        snprintf(esquerda, sizeof(esquerda), "Livres: %d", CAPACIDADE - veiculos);
  snprintf(l1, sizeof(l1), "%-11sE%c S%c", esquerda,
           cancelaEntrada.estaAberta() ? CH_ABERTA : CH_FECHADA,
           cancelaSaida.estaAberta()   ? CH_ABERTA : CH_FECHADA);

  lcdEscrever(0, l0);
  lcdEscrever(1, l1);
}

// ═════════════════════════════════════════════════════════════════════
//  Persistência (NVS)
// ═════════════════════════════════════════════════════════════════════
void nvsCarregar() {
  if (!PERSISTIR_DADOS) return;
  nvs.begin("estac", true);
  veiculos       = constrain(nvs.getInt("veic", 0), 0, CAPACIDADE);
  stats.entradas = nvs.getUInt("ent", 0);
  stats.saidas   = nvs.getUInt("sai", 0);
  stats.negados  = nvs.getUInt("neg", 0);
  stats.pico     = nvs.getInt("pico", 0);
  nvs.end();
}

void nvsSalvarSeNecessario() {
  static unsigned long ultimo = 0;
  if (!PERSISTIR_DADOS || !dadosAlterados || millis() - ultimo < NVS_INTERVALO_MS) return;
  ultimo = millis();
  nvs.begin("estac", false);
  nvs.putInt("veic", veiculos);
  nvs.putUInt("ent", stats.entradas);
  nvs.putUInt("sai", stats.saidas);
  nvs.putUInt("neg", stats.negados);
  nvs.putInt("pico", stats.pico);
  nvs.end();
  dadosAlterados = false;
}

// ═════════════════════════════════════════════════════════════════════
//  Serial: status e ajuda
// ═════════════════════════════════════════════════════════════════════
void imprimirStatus() {
  char buf[64];
  Serial.print("              ▕");
  for (int i = 0; i < CAPACIDADE; i++) Serial.print(i < veiculos ? "█" : "░");
  snprintf(buf, sizeof(buf), "▏ %2d/%d · %2d livres", veiculos, CAPACIDADE, CAPACIDADE - veiculos);
  Serial.print(buf);
  Serial.println(veiculos >= CAPACIDADE ? "  ● LOTADO" : "  ● VAGAS");
}

void imprimirAjuda() {
  Serial.println();
  Serial.println("  ┌─ Comandos do monitor serial ─────────────────────────┐");
  Serial.println("  │  E  entrada manual          S  saída manual          │");
  Serial.println("  │  T  contador = 29 (teste)   R  zera o contador       │");
  Serial.println("  │  ?  status completo         L  lista o histórico     │");
  Serial.println("  │  W  informações do Wi-Fi    M  liga/desliga o som    │");
  Serial.println("  │  Z  zera estatísticas       H  esta ajuda            │");
  Serial.println("  └──────────────────────────────────────────────────────┘");
  Serial.println();
}

void imprimirBanner() {
  Serial.println();
  Serial.println("╔════════════════════════════════════════════════════════╗");
  Serial.println("║   CONTROLE AUTOMATIZADO DE ESTACIONAMENTO · v2.0       ║");
  Serial.println("║   ESP32 · 30 vagas · LCD · HC-SR04 · Painel Web        ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");
  imprimirAjuda();
}

void imprimirStatusCompleto() {
  char buf[96];
  Serial.println();
  imprimirStatus();
  snprintf(buf, sizeof(buf), "  Cancelas : entrada %s · saída %s",
           nomeEstado(cancelaEntrada.estado), nomeEstado(cancelaSaida.estado));
  Serial.println(buf);
  snprintf(buf, sizeof(buf), "  Sensores : entrada %.0f cm%s · saída %.0f cm%s",
           sensorEntrada.distanciaCm, sensorEntrada.presente ? " (veículo)" : "",
           sensorSaida.distanciaCm,   sensorSaida.presente   ? " (veículo)" : "");
  Serial.println(buf);
  snprintf(buf, sizeof(buf), "  Totais   : %lu entradas · %lu saídas · %lu negados · pico %d",
           (unsigned long)stats.entradas, (unsigned long)stats.saidas,
           (unsigned long)stats.negados, stats.pico);
  Serial.println(buf);
  Serial.print("  Wi-Fi    : ");
  Serial.println(wifiConectado ? WiFi.localIP().toString() : String("desconectado"));
  Serial.println();
}

void imprimirHistorico() {
  Serial.println();
  Serial.println("  ── Últimos eventos ──");
  for (uint8_t i = 0; i < eventosTotal; i++) {
    const Evento& ev = eventos[(eventosInicio + i) % MAX_EVENTOS];
    Serial.print("  ");
    imprimirTimestamp(ev.instante);
    Serial.print(TIPO_ICONE[ev.tipo]);
    Serial.print(' ');
    Serial.println(ev.texto);
  }
  Serial.println();
}

// ═════════════════════════════════════════════════════════════════════
//  Sinalização e contador
// ═════════════════════════════════════════════════════════════════════
void atualizarLeds() {
  const bool lotado = (veiculos >= CAPACIDADE);
  digitalWrite(PIN_LED_VERMELHO, lotado ? HIGH : LOW);
  digitalWrite(PIN_LED_VERDE,    lotado ? LOW  : HIGH);
}

void definirVeiculos(int valor) {
  veiculos = constrain(valor, 0, CAPACIDADE);    // RF09
  if (veiculos > stats.pico) stats.pico = veiculos;
  dadosAlterados = true;
  atualizarLeds();
}

// ═════════════════════════════════════════════════════════════════════
//  Regras de negócio (origem: "sensor", "botão", "web", "serial")
// ═════════════════════════════════════════════════════════════════════
void processarEntrada(const char* origem) {
  if (veiculos >= CAPACIDADE) {
    stats.negados++;
    dadosAlterados = true;
    registrar(EV_NEGADO, "Acesso negado (%s): estacionamento LOTADO", origem);
    imprimirStatus();
    lcdAvisar("X ACESSO NEGADO");
    TOCAR(SOM_NEGADO);
    return;
  }
  definirVeiculos(veiculos + 1);
  stats.entradas++;
  cancelaEntrada.abrir();
  registrar(EV_ENTRADA, "Veículo autorizado (%s) - cancela ABERTA", origem);
  imprimirStatus();
  if (veiculos >= CAPACIDADE) {
    registrar(EV_ALERTA, "Capacidade máxima atingida - entrada bloqueada");
    lcdAvisar("! LOTOU  30/30");
    TOCAR(SOM_LOTADO);
  } else {
    lcdAvisar("> BEM-VINDO!");
    TOCAR(SOM_ENTRADA);
  }
}

void processarSaida(const char* origem) {
  if (veiculos <= 0) {
    registrar(EV_ALERTA, "Saída ignorada (%s): nenhum veículo", origem);
    imprimirStatus();
    lcdAvisar("! SEM VEICULOS");
    TOCAR(SOM_NEGADO);
    return;
  }
  const bool estavaLotado = (veiculos >= CAPACIDADE);
  definirVeiculos(veiculos - 1);
  stats.saidas++;
  cancelaSaida.abrir();
  registrar(EV_SAIDA, "Veículo liberado (%s) - cancela ABERTA", origem);
  imprimirStatus();
  if (estavaLotado) registrar(EV_ENTRADA, "Vaga liberada - entrada desbloqueada");
  lcdAvisar("< ATE LOGO!");
  TOCAR(SOM_SAIDA);
}

void tratarEventoCancela(Cancela& c, EventoCancela ev) {
  switch (ev) {
    case EventoCancela::FECHOU:
      registrar(EV_CANCELA, "Cancela de %s FECHADA", c.nome);
      break;
    case EventoCancela::OBSTRUIDA:
      registrar(EV_ALERTA, "Veículo sob a cancela de %s - fechamento adiado", c.nome);
      TOCAR(SOM_ALERTA);
      break;
    case EventoCancela::REABERTURA:
      registrar(EV_ALERTA, "Reabertura de segurança na cancela de %s", c.nome);
      TOCAR(SOM_ALERTA);
      break;
    case EventoCancela::NADA:
      break;
  }
}

// Medições alternadas: um sensor por vez evita que um capte o eco do outro
void agendarSensores() {
  static unsigned long ultimo = 0;
  static bool vezEntrada = true;
  if (millis() - ultimo < US_PERIODO_MS / 2) return;
  ultimo = millis();
  SensorUltrassonico& s = vezEntrada ? sensorEntrada : sensorSaida;
  if (!s.aguardandoEco) s.disparar();
  vezEntrada = !vezEntrada;
}

// ═════════════════════════════════════════════════════════════════════
//  Comandos (compartilhados pelo serial e pela web)
// ═════════════════════════════════════════════════════════════════════
bool executarComando(const String& acao, const char* origem) {
  if (acao == "entrada")            processarEntrada(origem);
  else if (acao == "saida")         processarSaida(origem);
  else if (acao == "abrir_entrada") { cancelaEntrada.abrir(); registrar(EV_CANCELA, "Abertura manual da entrada (%s)", origem); }
  else if (acao == "abrir_saida")   { cancelaSaida.abrir();   registrar(EV_CANCELA, "Abertura manual da saída (%s)", origem); }
  else if (acao == "teste")         { definirVeiculos(CAPACIDADE - 1); registrar(EV_TESTE, "Contador ajustado para %d (%s)", CAPACIDADE - 1, origem); imprimirStatus(); }
  else if (acao == "zerar")         { definirVeiculos(0); registrar(EV_TESTE, "Contador zerado (%s)", origem); imprimirStatus(); }
  else if (acao == "zerar_stats")   { stats = Estatisticas(); stats.pico = veiculos; dadosAlterados = true; registrar(EV_TESTE, "Estatísticas zeradas (%s)", origem); }
  else if (acao == "som")           { somAtivo = !somAtivo; if (!somAtivo) noTone(PIN_BUZZER); registrar(EV_SISTEMA, "Som %s (%s)", somAtivo ? "ligado" : "desligado", origem); }
  else return false;
  return true;
}

void processarSerial() {
  while (Serial.available() > 0) {
    switch (toupper(Serial.read())) {
      case 'E': executarComando("entrada", "serial");     break;
      case 'S': executarComando("saida", "serial");       break;
      case 'T': executarComando("teste", "serial");       break;
      case 'R': executarComando("zerar", "serial");       break;
      case 'Z': executarComando("zerar_stats", "serial"); break;
      case 'M': executarComando("som", "serial");         break;
      case '?': imprimirStatusCompleto();                 break;
      case 'L': imprimirHistorico();                      break;
      case 'H': imprimirAjuda();                          break;
      case 'W':
        Serial.print("  Wi-Fi: ");
        if (wifiConectado) {
          Serial.printf("%s · IP %s · RSSI %d dBm\n", WIFI_SSID,
                        WiFi.localIP().toString().c_str(), WiFi.RSSI());
        } else {
          Serial.println("conectando...");
        }
        break;
      default: break;
    }
  }
}

// ═════════════════════════════════════════════════════════════════════
//  Web: API JSON + página
// ═════════════════════════════════════════════════════════════════════
void jsonTexto(String& out, const char* s) {     // string JSON com escape
  out += '"';
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') { out += '\\'; out += *s; }
    else if ((uint8_t)*s < 0x20) out += ' ';
    else out += *s;
  }
  out += '"';
}

void jsonCancela(String& out, const Cancela& c) {
  out += "{\"estado\":\"";  out += nomeEstado(c.estado);
  out += "\",\"aberta\":";  out += c.estaAberta() ? "true" : "false";
  out += ",\"angulo\":";    out += c.angulo;
  out += ",\"obstruida\":"; out += c.obstruida() ? "true" : "false";
  out += ",\"aberturas\":"; out += c.aberturas;
  out += ",\"distancia\":"; out += String(c.sensor->distanciaCm, 0);
  out += '}';
}

String montarEstadoJson() {
  String j;
  j.reserve(2600);
  j += "{\"veiculos\":";    j += veiculos;
  j += ",\"capacidade\":";  j += CAPACIDADE;
  j += ",\"livres\":";      j += CAPACIDADE - veiculos;
  j += ",\"lotado\":";      j += veiculos >= CAPACIDADE ? "true" : "false";
  j += ",\"som\":";         j += somAtivo ? "true" : "false";
  j += ",\"uptime\":";      j += millis() / 1000;
  j += ",\"rssi\":";        j += wifiConectado ? WiFi.RSSI() : 0;
  j += ",\"cancelas\":{\"entrada\":"; jsonCancela(j, cancelaEntrada);
  j += ",\"saida\":";                 jsonCancela(j, cancelaSaida);
  j += "},\"stats\":{\"entradas\":";  j += stats.entradas;
  j += ",\"saidas\":";      j += stats.saidas;
  j += ",\"negados\":";     j += stats.negados;
  j += ",\"pico\":";        j += stats.pico;
  j += "},\"eventos\":[";
  for (int i = eventosTotal - 1; i >= 0; i--) {   // mais recente primeiro
    const Evento& ev = eventos[(eventosInicio + i) % MAX_EVENTOS];
    j += "{\"t\":";  j += ev.instante / 1000;
    j += ",\"tipo\":\""; j += TIPO_NOME[ev.tipo];
    j += "\",\"msg\":"; jsonTexto(j, ev.texto);
    j += '}';
    if (i > 0) j += ',';
  }
  j += "]}";
  return j;
}

void enviarEstado() {
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(200, "application/json", montarEstadoJson());
}

void configurarServidor() {
  servidor.on("/", HTTP_GET, []() {
    servidor.send(200, "text/html; charset=utf-8", PAGINA_HTML);
  });
  servidor.on("/api/estado", HTTP_GET, enviarEstado);
  servidor.on("/api/comando", HTTP_POST, []() {
    const String acao = servidor.arg("acao");
    if (!executarComando(acao, "web")) {
      servidor.send(400, "application/json", "{\"erro\":\"acao invalida\"}");
      return;
    }
    enviarEstado();
  });
  servidor.onNotFound([]() {
    servidor.send(404, "application/json", "{\"erro\":\"rota nao encontrada\"}");
  });
  servidor.begin();
  servidorAtivo = true;
}

// Conexão Wi-Fi não bloqueante: o sistema funciona mesmo sem rede
void atualizarWifi() {
  static unsigned long ultimo = 0;
  if (millis() - ultimo < 500) return;
  ultimo = millis();

  const bool conectado = (WiFi.status() == WL_CONNECTED);
  if (conectado && !wifiConectado) {
    wifiConectado = true;
    if (!servidorAtivo) {
      configurarServidor();
      MDNS.begin(MDNS_NOME);
    }
    const String ip = WiFi.localIP().toString();
    registrar(EV_SISTEMA, "Wi-Fi conectado - painel em http://%s", ip.c_str());
    char aviso[17];
    snprintf(aviso, sizeof(aviso), "IP %s", ip.c_str());
    lcdAvisar(aviso);
  } else if (!conectado && wifiConectado) {
    wifiConectado = false;
    registrar(EV_ALERTA, "Wi-Fi desconectado - tentando reconectar");
  }
}

// ═════════════════════════════════════════════════════════════════════
//  Setup & Loop
// ═════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(SERIAL_BAUD);

  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_VERMELHO, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  btnEntrada.iniciar();
  btnSaida.iniciar();
  sensorEntrada.iniciar();
  sensorSaida.iniciar();

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  cancelaEntrada.iniciar();
  cancelaSaida.iniciar();

  lcdIniciar();
  nvsCarregar();
  definirVeiculos(veiculos);
  dadosAlterados = false;

  imprimirBanner();
  registrar(EV_SISTEMA, "Sistema pronto - %d/%d veículos%s", veiculos, CAPACIDADE,
            (PERSISTIR_DADOS && veiculos > 0) ? " (restaurado da flash)" : "");
  imprimirStatus();
  TOCAR(SOM_INICIO);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_SENHA, WIFI_CANAL);
  registrar(EV_SISTEMA, "Conectando ao Wi-Fi \"%s\"...", WIFI_SSID);
}

void loop() {
  // 1. Entradas: botões manuais (debounce) e sensores ultrassônicos
  if (btnEntrada.foiPressionado()) processarEntrada("botão");
  if (btnSaida.foiPressionado())   processarSaida("botão");

  agendarSensores();
  if (sensorEntrada.atualizar() == SENSOR_CHEGOU) processarEntrada("sensor");
  if (sensorSaida.atualizar()   == SENSOR_CHEGOU) processarSaida("sensor");

  // 2. Cancelas (movimento + temporização + segurança)
  tratarEventoCancela(cancelaEntrada, cancelaEntrada.atualizar());
  tratarEventoCancela(cancelaSaida,   cancelaSaida.atualizar());

  // 3. Interfaces
  buzzer.atualizar();
  lcdAtualizar();
  processarSerial();
  atualizarWifi();
  if (servidorAtivo) servidor.handleClient();

  // 4. Persistência
  nvsSalvarSeNecessario();
}
