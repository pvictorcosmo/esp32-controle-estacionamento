/*
 * ════════════════════════════════════════════════════════════════════
 *   componentes.h — Tipos do sistema
 *   Botão, sensor HC-SR04, cancela, buzzer e eventos.
 *   (Ficam num header para o gerador de protótipos do Arduino
 *    enxergar os tipos antes das funções do sketch.)
 * ════════════════════════════════════════════════════════════════════
 */
#pragma once
#include <Arduino.h>
#include <ESP32Servo.h>
#include "config.h"

extern bool somAtivo;   // definido no sketch.ino

// ═════════════════════════════════════════════════════════════════════
//  Eventos
// ═════════════════════════════════════════════════════════════════════
enum TipoEvento : uint8_t { EV_SISTEMA, EV_ENTRADA, EV_SAIDA, EV_NEGADO, EV_ALERTA, EV_CANCELA, EV_TESTE };

const char* const TIPO_NOME[]  = { "sistema", "entrada", "saida", "negado", "alerta", "cancela", "teste" };
const char* const TIPO_ICONE[] = { "🔧", "🟢", "🔵", "⛔", "🚨", "🔒", "🧪" };
const char* const TIPO_TAG[]   = { "SISTEMA", "ENTRADA", "SAÍDA  ", "NEGADO ", "ALERTA ", "CANCELA", "TESTE  " };

struct Evento {
  unsigned long instante;
  TipoEvento    tipo;
  char          texto[64];
};

// ═════════════════════════════════════════════════════════════════════
//  Botão com debounce + detecção de borda
// ═════════════════════════════════════════════════════════════════════
struct Botao {
  uint8_t       pino;
  int           estadoEstavel   = HIGH;
  int           ultimaLeitura   = HIGH;
  unsigned long instanteMudanca = 0;

  explicit Botao(uint8_t p) : pino(p) {}

  void iniciar() {
    pinMode(pino, INPUT_PULLUP);
    estadoEstavel = ultimaLeitura = digitalRead(pino);
    instanteMudanca = millis();
  }

  bool foiPressionado() {
    const int leitura = digitalRead(pino);
    const unsigned long agora = millis();
    if (leitura != ultimaLeitura) {
      ultimaLeitura   = leitura;
      instanteMudanca = agora;
    }
    if ((agora - instanteMudanca) >= DEBOUNCE_MS && leitura != estadoEstavel) {
      estadoEstavel = leitura;
      return estadoEstavel == LOW;
    }
    return false;
  }
};

// ═════════════════════════════════════════════════════════════════════
//  Sensor ultrassônico HC-SR04 — eco medido por interrupção
// ═════════════════════════════════════════════════════════════════════
enum EventoSensor : int8_t { SENSOR_NADA = 0, SENSOR_CHEGOU = 1, SENSOR_SAIU = -1 };

struct SensorUltrassonico {
  const char* nome;
  uint8_t     pinoTrig;
  uint8_t     pinoEcho;

  volatile unsigned long inicioEco  = 0;
  volatile unsigned long duracaoEco = 0;
  volatile bool          ecoPronto  = false;

  unsigned long instanteDisparo = 0;
  bool          aguardandoEco   = false;
  float         distanciaCm     = US_FORA_ALCANCE_CM;
  bool          presente        = false;
  uint8_t       confirmacoes    = 0;

  SensorUltrassonico(const char* n, uint8_t trig, uint8_t echo)
    : nome(n), pinoTrig(trig), pinoEcho(echo) {}

  static void IRAM_ATTR aoMudarEco(void* arg) {
    SensorUltrassonico* s = static_cast<SensorUltrassonico*>(arg);
    if (digitalRead(s->pinoEcho) == HIGH) {
      s->inicioEco = micros();
    } else {
      s->duracaoEco = micros() - s->inicioEco;
      s->ecoPronto  = true;
    }
  }

  void iniciar() {
    pinMode(pinoTrig, OUTPUT);
    digitalWrite(pinoTrig, LOW);
    pinMode(pinoEcho, INPUT);
    attachInterruptArg(digitalPinToInterrupt(pinoEcho), aoMudarEco, this, CHANGE);
  }

  void disparar() {
    ecoPronto = false;
    digitalWrite(pinoTrig, HIGH);
    delayMicroseconds(10);              // pulso de 10 µs exigido pelo HC-SR04
    digitalWrite(pinoTrig, LOW);
    instanteDisparo = millis();
    aguardandoEco   = true;
  }

  // Chamado a cada loop. Retorna CHEGOU/SAIU quando o estado muda.
  EventoSensor atualizar() {
    if (!aguardandoEco) return SENSOR_NADA;

    float d;
    if (ecoPronto) {
      d = duracaoEco / 58.0f;                       // µs → cm
      if (d > US_FORA_ALCANCE_CM) d = US_FORA_ALCANCE_CM;
    } else if (millis() - instanteDisparo >= US_TIMEOUT_MS) {
      d = US_FORA_ALCANCE_CM;                       // sem eco
    } else {
      return SENSOR_NADA;                           // ainda medindo
    }
    aguardandoEco = false;
    distanciaCm   = d;
    return filtrar(d);
  }

  // Histerese (50 cm entra / 70 cm sai) + N leituras seguidas
  EventoSensor filtrar(float d) {
    const bool leitura = presente ? (d < US_LIMIAR_CM + US_HISTERESE_CM)
                                  : (d < US_LIMIAR_CM);
    if (leitura == presente) { confirmacoes = 0; return SENSOR_NADA; }
    if (++confirmacoes < US_CONFIRMACOES) return SENSOR_NADA;
    confirmacoes = 0;
    presente     = leitura;
    return presente ? SENSOR_CHEGOU : SENSOR_SAIU;
  }
};

// ═════════════════════════════════════════════════════════════════════
//  Cancela — máquina de estados com movimento suave e segurança
// ═════════════════════════════════════════════════════════════════════
enum class EstadoCancela : uint8_t { FECHADA, ABRINDO, ABERTA, FECHANDO };
enum class EventoCancela : uint8_t { NADA, FECHOU, OBSTRUIDA, REABERTURA };

inline const char* nomeEstado(EstadoCancela e) {
  switch (e) {
    case EstadoCancela::FECHADA:  return "fechada";
    case EstadoCancela::ABRINDO:  return "abrindo";
    case EstadoCancela::ABERTA:   return "aberta";
    case EstadoCancela::FECHANDO: return "fechando";
  }
  return "?";
}

struct Cancela {
  const char*         nome;
  uint8_t             pino;
  SensorUltrassonico* sensor;          // detecta veículo sob a cancela

  Servo         servo;
  EstadoCancela estado           = EstadoCancela::FECHADA;
  int           angulo           = ANGULO_FECHADA;
  unsigned long instanteAbertura = 0;
  unsigned long ultimoPasso      = 0;
  bool          avisouObstrucao  = false;
  uint32_t      aberturas        = 0;

  Cancela(const char* n, uint8_t p, SensorUltrassonico* s) : nome(n), pino(p), sensor(s) {}

  void iniciar() {
    servo.setPeriodHertz(50);
    servo.attach(pino, SERVO_PULSO_MIN, SERVO_PULSO_MAX);
    servo.write(ANGULO_FECHADA);
  }

  bool estaAberta() const { return estado != EstadoCancela::FECHADA; }
  bool obstruida()  const { return sensor != nullptr && sensor->presente; }

  // Abre (ou mantém aberta) e reinicia a contagem de 2,5 s
  void abrir() {
    instanteAbertura = millis();
    avisouObstrucao  = false;
    if (estado != EstadoCancela::ABERTA) estado = EstadoCancela::ABRINDO;
    aberturas++;
  }

  // Move o servo um passo em direção ao alvo; true quando chegou
  bool moverPara(int alvo) {
    const unsigned long agora = millis();
    if (angulo == alvo) return true;
    if (agora - ultimoPasso < SERVO_PASSO_MS) return false;
    ultimoPasso = agora;
    angulo += (alvo > angulo) ? SERVO_PASSO_GRAUS : -SERVO_PASSO_GRAUS;
    if ((alvo > ANGULO_FECHADA && angulo > alvo) || (alvo == ANGULO_FECHADA && angulo < alvo)) angulo = alvo;
    servo.write(angulo);
    return angulo == alvo;
  }

  EventoCancela atualizar() {
    const unsigned long agora = millis();
    switch (estado) {
      case EstadoCancela::FECHADA:
        break;

      case EstadoCancela::ABRINDO:
        if (moverPara(ANGULO_ABERTA)) estado = EstadoCancela::ABERTA;
        break;

      case EstadoCancela::ABERTA:
        if (agora - instanteAbertura >= TEMPO_ABERTA_MS) {
          if (obstruida()) {                       // veículo ainda embaixo
            if (!avisouObstrucao) { avisouObstrucao = true; return EventoCancela::OBSTRUIDA; }
          } else {
            estado = EstadoCancela::FECHANDO;
          }
        }
        break;

      case EstadoCancela::FECHANDO:
        if (obstruida()) {                         // apareceu veículo: reabre
          estado           = EstadoCancela::ABRINDO;
          instanteAbertura = agora;
          avisouObstrucao  = false;
          return EventoCancela::REABERTURA;
        }
        if (moverPara(ANGULO_FECHADA)) {
          estado = EstadoCancela::FECHADA;
          return EventoCancela::FECHOU;
        }
        break;
    }
    return EventoCancela::NADA;
  }
};

// ═════════════════════════════════════════════════════════════════════
//  Buzzer — sequenciador de notas não bloqueante
// ═════════════════════════════════════════════════════════════════════
struct Nota { uint16_t freq; uint16_t ms; };   // freq 0 = pausa

const Nota SOM_INICIO[]  = { {1047, 80}, {1319, 80}, {1568, 140} };
const Nota SOM_ENTRADA[] = { {1568, 70}, {0, 40}, {2093, 90} };
const Nota SOM_SAIDA[]   = { {2093, 70}, {0, 40}, {1568, 90} };
const Nota SOM_NEGADO[]  = { {392, 180}, {0, 80}, {392, 180} };
const Nota SOM_LOTADO[]  = { {880, 120}, {0, 50}, {660, 120}, {0, 50}, {440, 220} };
const Nota SOM_ALERTA[]  = { {1760, 60}, {0, 60}, {1760, 60} };

#define TOCAR(som) buzzer.tocar(som, sizeof(som) / sizeof(som[0]))

struct Buzzer {
  uint8_t       pino;
  const Nota*   sequencia = nullptr;
  uint8_t       total     = 0;
  uint8_t       indice    = 0;
  unsigned long inicio    = 0;

  explicit Buzzer(uint8_t p) : pino(p) {}

  void tocar(const Nota* seq, uint8_t n) {
    if (!somAtivo) return;
    sequencia = seq; total = n; indice = 0;
    iniciarNota();
  }

  void iniciarNota() {
    inicio = millis();
    if (sequencia[indice].freq) tone(pino, sequencia[indice].freq);
    else noTone(pino);
  }

  void atualizar() {
    if (!sequencia) return;
    if (millis() - inicio < sequencia[indice].ms) return;
    if (++indice >= total) { noTone(pino); sequencia = nullptr; }
    else iniciarNota();
  }
};

