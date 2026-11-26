# 🚗 Controle Automatizado de Estacionamento — ESP32

Sistema embarcado que controla a entrada e a saída de veículos de um estacionamento com **30 vagas**, usando duas cancelas (servos SG90), dois botões, LEDs de sinalização e o monitor serial.

`ESP32 DevKit V1` · `Wokwi` · `C/C++ (Arduino)` · `ESP32Servo`

---

## 📦 Arquivos

| Arquivo         | Conteúdo                                          |
|-----------------|---------------------------------------------------|
| `sketch.ino`    | Firmware completo                                 |
| `diagram.json`  | Circuito do Wokwi (ESP32, botões, LEDs, servos)   |
| `libraries.txt` | Bibliotecas necessárias (ESP32Servo)              |
| `README.md`     | Este guia                                         |

## 🔌 Mapeamento dos GPIOs

| Dispositivo    | GPIO | Função                        |
|----------------|:----:|-------------------------------|
| Botão ENTRADA  | 18   | Entrada digital (INPUT_PULLUP)|
| Botão SAÍDA    | 19   | Entrada digital (INPUT_PULLUP)|
| LED verde      | 21   | Saída digital (via 220 Ω)     |
| LED vermelho   | 22   | Saída digital (via 220 Ω)     |
| Servo entrada  | 25   | PWM 50 Hz                     |
| Servo saída    | 26   | PWM 50 Hz                     |

Botões ligados entre o GPIO e o GND. Servos alimentados em 5 V (VIN). Em montagem física, use fonte externa de 5 V para os servos com **GND comum** ao ESP32.

## ▶️ Como executar no Wokwi

1. Crie um projeto **ESP32** em [wokwi.com](https://wokwi.com).
2. Substitua `sketch.ino` pelo firmware deste pacote.
3. Substitua `diagram.json` pelo arquivo deste pacote.
4. No **Library Manager**, adicione **ESP32Servo** (ou crie `libraries.txt`).
5. Clique em **Start Simulation** e abra o monitor serial.
6. Use os botões **ENTRADA**/**SAÍDA** ou as teclas **E**/**S** com a simulação em foco.

## ⌨️ Comandos do monitor serial

| Tecla | Ação                                   |
|:-----:|----------------------------------------|
| `T`   | Ajusta o contador para 29 veículos     |
| `R`   | Zera o contador                        |
| `?`   | Mostra o status atual                  |
| `H`   | Mostra a ajuda                         |

## 🧠 Como o firmware funciona

- **Debounce de 40 ms + detecção de borda** (`struct Botao`): o evento só dispara quando o nível LOW fica estável por 40 ms, e só uma vez por acionamento — segurar o botão não gera novas contagens (RF11).
- **Cancelas independentes** (`struct Cancela`): cada uma tem seu próprio servo e seu próprio instante de abertura. O `loop()` verifica `millis()` e fecha cada cancela após **2,5 s**, sem `delay()` (RF10, RF12).
- **Contador protegido**: toda alteração passa por `definirVeiculos()`, que aplica `constrain(0, 30)` e atualiza os LEDs (RF09, RF04, RF05).
- **Uma passagem por abertura**: enquanto uma cancela está aberta, novos pedidos naquela cancela são ignorados ("aguarde o veículo passar"), evitando contar dois carros numa única abertura.

### Exemplo de saída serial

```
[00:05.370] 🧪 TESTE   Contador ajustado para 29 veículos.
             ▕█████████████████████████████░▏ 29/30 ocupadas ·  1 livres  ● VAGAS
[00:05.423] 🟢 ENTRADA Veículo autorizado — cancela de entrada ABERTA.
             ▕██████████████████████████████▏ 30/30 ocupadas ·  0 livres  ● LOTADO
[00:05.423] 🔴 AVISO   Capacidade máxima atingida. Entrada bloqueada.
[00:07.923] 🔒 ENTRADA Cancela de entrada FECHADA.
[00:08.103] ⛔ ENTRADA Acesso negado — estacionamento LOTADO.
```

## ✅ Plano de testes

| Teste | Procedimento                         | Resultado esperado                 |
|:-----:|--------------------------------------|------------------------------------|
| TF01  | Iniciar o sistema                    | Contador = 0, LED verde ligado     |
| TF02  | Pressionar ENTRADA                   | Contador = 1, servo entrada abre/fecha |
| TF03  | Pressionar SAÍDA                     | Contador = 0, servo saída abre/fecha |
| TF04  | Enviar `T`                           | Contador = 29                      |
| TF05  | Pressionar ENTRADA após `T`          | Contador = 30, LED vermelho ligado |
| TF06  | Tentar nova entrada                  | Acesso negado                      |
| TF07  | Pressionar SAÍDA com lotação máxima  | Contador = 29, LED verde ligado    |
| TF08  | Enviar `R` e pressionar SAÍDA        | Contador permanece 0               |
| TF09  | Manter ENTRADA pressionado           | Apenas uma entrada registrada      |
| TF10  | ENTRADA e SAÍDA em sequência rápida  | Cancelas operam independentemente  |

## 🚀 Próximas atividades

1. **LCD 16×2 I2C** — mostrar vagas ocupadas e livres.
2. **HC-SR04** — substituir os botões por sensores ultrassônicos.
3. **Monitoramento Web** — página local via Wi-Fi com vagas e estado das cancelas.
