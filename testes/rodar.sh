#!/usr/bin/env bash
# Roda os testes funcionais no PC (g++), simulando pinos, tempo, sensores,
# LCD, Wi-Fi e servidor web com "mocks". Não precisa de ESP32 nem do Wokwi.
set -e
cd "$(dirname "$0")"
for v in referencia completa; do
  echo "══ Versão: $v ══"
  ( cd "$v" && g++ -std=gnu++11 -I. main.cpp -o /tmp/teste_$v && /tmp/teste_$v | grep -E "PASS|FAIL|fails" )
done
