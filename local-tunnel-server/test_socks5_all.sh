#!/bin/bash
# Скрипт для тестирования SOCKS5 proxy с различными приложениями

echo "=== Тест SOCKS5 Proxy ==="

# Настройки
PROXY_HOST="127.0.0.1"
PROXY_PORT="8084"
TEST_URL="http://httpbin.org/ip"

echo "Прокси: $PROXY_HOST:$PROXY_PORT"
echo "Тестовый URL: $TEST_URL"
echo

# Тест 1: curl через SOCKS5
echo "🔍 Тест 1: curl через SOCKS5"
echo "Команда: curl --socks5 $PROXY_HOST:$PROXY_PORT $TEST_URL"
echo
curl --socks5 "$PROXY_HOST:$PROXY_PORT" "$TEST_URL" --connect-timeout 10 -v
echo
echo "Код выхода curl: $?"
echo

# Тест 2: Python тест
echo "🔍 Тест 2: Python SOCKS5 тест"
echo "Команда: python3 test_socks5.py $PROXY_HOST $PROXY_PORT httpbin.org 80"
echo
python3 ../test_socks5.py "$PROXY_HOST" "$PROXY_PORT" "httpbin.org" "80"
echo
echo "Код выхода Python: $?"
echo

# Тест 3: Простое подключение через telnet
echo "🔍 Тест 3: Telnet подключение"
echo "Пробуем подключиться через telnet..."
timeout 5 telnet "$PROXY_HOST" "$PROXY_PORT" <<EOF
echo "Подключение установлено"
EOF
echo "Код выхода telnet: $?"
echo

echo "=== Завершение тестов ==="