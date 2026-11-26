/*
 * ════════════════════════════════════════════════════════════════════
 *   config.h — Pinos, parâmetros e credenciais
 *   Todas as constantes ajustáveis do projeto ficam aqui.
 * ════════════════════════════════════════════════════════════════════
 */
#pragma once
#include <Arduino.h>

// ─────────────────────────── Rede Wi-Fi ──────────────────────────────
// No Wokwi use "Wokwi-GUEST" sem senha (canal 6 acelera a conexão).
#define WIFI_SSID      "Wokwi-GUEST"
#define WIFI_SENHA     ""
#define WIFI_CANAL     6
#define MDNS_NOME      "estacionamento"   // http://estacionamento.local (hardware real)

// ───────────────────────────── Pinos ─────────────────────────────────
// Botões manuais do operador (ligados ao GND, INPUT_PULLUP)
constexpr uint8_t PIN_BTN_ENTRADA   = 18;
constexpr uint8_t PIN_BTN_SAIDA     = 19;

// Display LCD 16×2 I2C (barramento padrão do ESP32)
constexpr uint8_t PIN_I2C_SDA       = 21;
constexpr uint8_t PIN_I2C_SCL       = 22;
constexpr uint8_t LCD_ENDERECO      = 0x27;

// LEDs (saíram de 21/22 para liberar o I2C do LCD)
constexpr uint8_t PIN_LED_VERDE     = 32;
constexpr uint8_t PIN_LED_VERMELHO  = 33;

// Servos das cancelas (PWM 50 Hz)
constexpr uint8_t PIN_SERVO_ENTRADA = 25;
constexpr uint8_t PIN_SERVO_SAIDA   = 26;

// Sensores ultrassônicos HC-SR04
constexpr uint8_t PIN_US_ENT_TRIG   = 13;
constexpr uint8_t PIN_US_ENT_ECHO   = 14;
constexpr uint8_t PIN_US_SAI_TRIG   = 27;
constexpr uint8_t PIN_US_SAI_ECHO   = 34;   // GPIO34 é só entrada: ideal p/ ECHO

// Buzzer passivo (feedback sonoro)
constexpr uint8_t PIN_BUZZER        = 23;

// ─────────────────────────── Regras ──────────────────────────────────
constexpr int           CAPACIDADE         = 30;     // vagas
constexpr unsigned long DEBOUNCE_MS        = 40;     // botões
constexpr unsigned long TEMPO_ABERTA_MS    = 2500;   // cancela aberta

// ─────────────────────────── Cancelas ────────────────────────────────
constexpr int           ANGULO_FECHADA     = 0;
constexpr int           ANGULO_ABERTA      = 90;
constexpr int           SERVO_PULSO_MIN    = 500;    // µs (SG90)
constexpr int           SERVO_PULSO_MAX    = 2400;   // µs (SG90)
constexpr unsigned long SERVO_PASSO_MS     = 10;     // movimento suave:
constexpr int           SERVO_PASSO_GRAUS  = 3;      //  ~300 ms p/ 90°

// ───────────────────────── Sensores HC-SR04 ──────────────────────────
constexpr unsigned long US_PERIODO_MS      = 60;     // 1 medição/sensor a cada 60 ms
constexpr unsigned long US_TIMEOUT_MS      = 40;     // sem eco → fora de alcance
constexpr float         US_LIMIAR_CM       = 50.0f;  // < 50 cm → veículo presente
constexpr float         US_HISTERESE_CM    = 20.0f;  // > 70 cm → veículo saiu
constexpr uint8_t       US_CONFIRMACOES    = 3;      // leituras seguidas p/ confirmar
constexpr float         US_FORA_ALCANCE_CM = 400.0f;

// ─────────────────────────── Persistência ────────────────────────────
constexpr unsigned long NVS_INTERVALO_MS   = 2000;   // grava no máx. a cada 2 s

// ─────────────────────────── Interface ───────────────────────────────
constexpr uint32_t      SERIAL_BAUD        = 115200;
constexpr unsigned long LCD_MENSAGEM_MS    = 2000;   // duração dos avisos no LCD
constexpr uint8_t       MAX_EVENTOS        = 15;     // histórico em memória

// ─────────────────────────── Opções ──────────────────────────────────
constexpr bool          PERSISTIR_DADOS    = true;   // salva contador/estatísticas na flash
