/*
 * ════════════════════════════════════════════════════════════════════
 *   CONTROLE AUTOMATIZADO DE ESTACIONAMENTO — ESP32 DevKit V1
 * ════════════════════════════════════════════════════════════════════
 *   Laboratório de Sistemas Embarcados · Wokwi · C/C++ (Arduino)
 *
 *   • Capacidade: 30 vagas
 *   • 2 cancelas independentes (servos SG90) — 0° fechada / 90° aberta
 *   • LED verde = há vagas · LED vermelho = lotado
 *   • Debounce de 40 ms com detecção de borda (1 acionamento = 1 evento)
 *   • Fechamento automático após 2,5 s usando millis() (não bloqueante)
 *   • Monitor serial (115200 bps) com comandos de teste:
 *        T → ajusta o contador para 29   R → zera o contador
 *        ? → mostra o status atual        H → ajuda
 *
 *   Mapeamento de GPIOs
 *   ┌──────────────────┬──────┬────────────────────────────┐
 *   │ Botão ENTRADA    │  18  │ Entrada digital (pull-up)  │
 *   │ Botão SAÍDA      │  19  │ Entrada digital (pull-up)  │
 *   │ LED verde        │  21  │ Saída digital              │
 *   │ LED vermelho     │  22  │ Saída digital              │
 *   │ Servo entrada    │  25  │ PWM                        │
 *   │ Servo saída      │  26  │ PWM                        │
 *   └──────────────────┴──────┴────────────────────────────┘
 * ════════════════════════════════════════════════════════════════════
 */

#include <ESP32Servo.h>

// ─────────────────────────── Pinos ───────────────────────────────────
constexpr uint8_t PIN_BTN_ENTRADA   = 18;
constexpr uint8_t PIN_BTN_SAIDA     = 19;
constexpr uint8_t PIN_LED_VERDE     = 21;
constexpr uint8_t PIN_LED_VERMELHO  = 22;
constexpr uint8_t PIN_SERVO_ENTRADA = 25;
constexpr uint8_t PIN_SERVO_SAIDA   = 26;

// ───────────────────────── Parâmetros ────────────────────────────────
constexpr int           CAPACIDADE       = 30;    // vagas
constexpr unsigned long DEBOUNCE_MS      = 40;    // ms
constexpr unsigned long TEMPO_ABERTA_MS  = 2500;  // ms
constexpr int           ANGULO_FECHADA   = 0;     // graus
constexpr int           ANGULO_ABERTA    = 90;    // graus
constexpr int           SERVO_PULSO_MIN  = 500;   // µs (SG90)
constexpr int           SERVO_PULSO_MAX  = 2400;  // µs (SG90)
constexpr uint32_t      SERIAL_BAUD      = 115200;

// ─────────────────────────── Estado ──────────────────────────────────
int veiculos = 0;   // veículos atualmente no estacionamento

// ═════════════════════════════════════════════════════════════════════
//  Botão com debounce + detecção de borda de descida (pressionar)
// ═════════════════════════════════════════════════════════════════════
struct Botao {
  uint8_t       pino;
  int           estadoEstavel   = HIGH; // último estado validado pelo debounce
  int           ultimaLeitura   = HIGH; // última leitura bruta
  unsigned long instanteMudanca = 0;    // quando a leitura bruta mudou

  explicit Botao(uint8_t p) : pino(p) {}

  void iniciar() {
    pinMode(pino, INPUT_PULLUP);
    estadoEstavel   = digitalRead(pino);
    ultimaLeitura   = estadoEstavel;
    instanteMudanca = millis();
  }

  // Retorna true UMA única vez por acionamento (borda HIGH → LOW estável).
  // Manter o botão pressionado não gera novos eventos.
  bool foiPressionado() {
    const int leitura = digitalRead(pino);
    const unsigned long agora = millis();

    if (leitura != ultimaLeitura) {          // houve oscilação: reinicia janela
      ultimaLeitura   = leitura;
      instanteMudanca = agora;
    }

    if ((agora - instanteMudanca) >= DEBOUNCE_MS && leitura != estadoEstavel) {
      estadoEstavel = leitura;               // nível estável por ≥ 40 ms
      return estadoEstavel == LOW;           // LOW = pressionado (pull-up)
    }
    return false;
  }
};

// ═════════════════════════════════════════════════════════════════════
//  Cancela: servo + temporizador próprio (independente da outra)
// ═════════════════════════════════════════════════════════════════════
struct Cancela {
  const char*   nome;
  uint8_t       pino;
  Servo         servo;
  bool          aberta           = false;
  unsigned long instanteAbertura = 0;

  Cancela(const char* n, uint8_t p) : nome(n), pino(p) {}

  void iniciar() {
    servo.setPeriodHertz(50);                        // SG90 → 50 Hz
    servo.attach(pino, SERVO_PULSO_MIN, SERVO_PULSO_MAX);
    servo.write(ANGULO_FECHADA);
    aberta = false;
  }

  void abrir() {
    servo.write(ANGULO_ABERTA);
    aberta           = true;
    instanteAbertura = millis();
  }

  void fechar() {
    servo.write(ANGULO_FECHADA);
    aberta = false;
  }

  // Deve ser chamada a cada loop: fecha sozinha após 2,5 s, sem delay().
  // Retorna true no instante em que fechou.
  bool atualizar() {
    if (aberta && (millis() - instanteAbertura) >= TEMPO_ABERTA_MS) {
      fechar();
      return true;
    }
    return false;
  }
};

Botao   btnEntrada     { PIN_BTN_ENTRADA };
Botao   btnSaida       { PIN_BTN_SAIDA };
Cancela cancelaEntrada { "ENTRADA", PIN_SERVO_ENTRADA };
Cancela cancelaSaida   { "SAIDA",   PIN_SERVO_SAIDA };

// ═════════════════════════════════════════════════════════════════════
//  Saída serial estilizada
// ═════════════════════════════════════════════════════════════════════
void imprimirTimestamp() {
  const unsigned long ms = millis();
  char buf[16];
  snprintf(buf, sizeof(buf), "[%02lu:%02lu.%03lu] ",
           (ms / 60000UL) % 100, (ms / 1000UL) % 60, ms % 1000UL);
  Serial.print(buf);
}

// Barra de ocupação: um bloco por vaga (█ ocupada · ░ livre)
void imprimirBarra() {
  Serial.print("▕");
  for (int i = 0; i < CAPACIDADE; i++) Serial.print(i < veiculos ? "█" : "░");
  Serial.print("▏");
}

void imprimirStatus() {
  const int livres = CAPACIDADE - veiculos;
  char buf[48];
  Serial.print("             ");
  imprimirBarra();
  snprintf(buf, sizeof(buf), " %2d/%d ocupadas · %2d livres", veiculos, CAPACIDADE, livres);
  Serial.print(buf);
  Serial.println(veiculos >= CAPACIDADE ? "  ● LOTADO" : "  ● VAGAS");
}

void registrar(const char* tag, const char* mensagem) {
  imprimirTimestamp();
  Serial.print(tag);
  Serial.print(' ');
  Serial.println(mensagem);
}

void imprimirAjuda() {
  Serial.println();
  Serial.println("  Comandos do monitor serial:");
  Serial.println("    T  →  ajusta o contador para 29 veículos (teste de lotação)");
  Serial.println("    R  →  zera o contador");
  Serial.println("    ?  →  mostra o status atual");
  Serial.println("    H  →  mostra esta ajuda");
  Serial.println("  No Wokwi, com a simulação em foco: tecla E = ENTRADA · S = SAÍDA");
  Serial.println();
}

void imprimirBanner() {
  Serial.println();
  Serial.println("╔════════════════════════════════════════════════════╗");
  Serial.println("║      CONTROLE AUTOMATIZADO DE ESTACIONAMENTO       ║");
  Serial.println("║               ESP32 · 30 vagas · Wokwi             ║");
  Serial.println("╚════════════════════════════════════════════════════╝");
  imprimirAjuda();
}

// ═════════════════════════════════════════════════════════════════════
//  Sinalização
// ═════════════════════════════════════════════════════════════════════
void atualizarLeds() {
  const bool lotado = (veiculos >= CAPACIDADE);
  digitalWrite(PIN_LED_VERMELHO, lotado ? HIGH : LOW);
  digitalWrite(PIN_LED_VERDE,    lotado ? LOW  : HIGH);
}

// Garante 0 ≤ veiculos ≤ CAPACIDADE (RF09) e atualiza a sinalização
void definirVeiculos(int valor) {
  veiculos = constrain(valor, 0, CAPACIDADE);
  atualizarLeds();
}

// ═════════════════════════════════════════════════════════════════════
//  Regras de negócio
// ═════════════════════════════════════════════════════════════════════
void processarEntrada() {
  if (veiculos >= CAPACIDADE) {
    registrar("⛔ ENTRADA", "Acesso negado — estacionamento LOTADO.");
    imprimirStatus();
    return;
  }
  if (cancelaEntrada.aberta) {
    registrar("⏳ ENTRADA", "Cancela já aberta — aguarde o veículo passar.");
    return;
  }
  definirVeiculos(veiculos + 1);
  cancelaEntrada.abrir();
  registrar("🟢 ENTRADA", "Veículo autorizado — cancela de entrada ABERTA.");
  imprimirStatus();
  if (veiculos >= CAPACIDADE) {
    registrar("🔴 AVISO  ", "Capacidade máxima atingida. Entrada bloqueada.");
  }
}

void processarSaida() {
  if (veiculos <= 0) {
    registrar("🚫 SAÍDA  ", "Nenhum veículo no estacionamento — saída ignorada.");
    imprimirStatus();
    return;
  }
  if (cancelaSaida.aberta) {
    registrar("⏳ SAÍDA  ", "Cancela já aberta — aguarde o veículo passar.");
    return;
  }
  const bool estavaLotado = (veiculos >= CAPACIDADE);
  definirVeiculos(veiculos - 1);
  cancelaSaida.abrir();
  registrar("🔵 SAÍDA  ", "Veículo liberado — cancela de saída ABERTA.");
  imprimirStatus();
  if (estavaLotado) {
    registrar("🟢 AVISO  ", "Vaga liberada. Entrada desbloqueada.");
  }
}

void processarSerial() {
  while (Serial.available() > 0) {
    const char c = toupper(Serial.read());
    switch (c) {
      case 'T':
        definirVeiculos(CAPACIDADE - 1);
        registrar("🧪 TESTE  ", "Contador ajustado para 29 veículos.");
        imprimirStatus();
        break;
      case 'R':
        definirVeiculos(0);
        registrar("🧪 TESTE  ", "Contador zerado.");
        imprimirStatus();
        break;
      case '?':
        registrar("📊 STATUS ", "");
        imprimirStatus();
        break;
      case 'H':
        imprimirAjuda();
        break;
      default:
        break;   // ignora \r, \n e caracteres desconhecidos
    }
  }
}

// ═════════════════════════════════════════════════════════════════════
//  Setup & Loop
// ═════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(SERIAL_BAUD);

  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_VERMELHO, OUTPUT);

  btnEntrada.iniciar();
  btnSaida.iniciar();

  // Reserva timers de hardware para o PWM dos servos
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  cancelaEntrada.iniciar();
  cancelaSaida.iniciar();

  definirVeiculos(0);

  imprimirBanner();
  registrar("✅ SISTEMA", "Pronto. Cancelas fechadas.");
  imprimirStatus();
}

void loop() {
  // 1. Botões (debounce + borda) — cada um tratado de forma independente
  if (btnEntrada.foiPressionado()) processarEntrada();
  if (btnSaida.foiPressionado())   processarSaida();

  // 2. Temporização das cancelas (millis, não bloqueante)
  if (cancelaEntrada.atualizar()) registrar("🔒 ENTRADA", "Cancela de entrada FECHADA.");
  if (cancelaSaida.atualizar())   registrar("🔒 SAÍDA  ", "Cancela de saída FECHADA.");

  // 3. Comandos de teste pelo monitor serial
  processarSerial();
}
