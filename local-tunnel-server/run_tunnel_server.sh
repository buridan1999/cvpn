#!/bin/bash

# Запуск только Tunnel сервера (локальный прокси)
# Порт 8084 - принимает HTTP/SOCKS5 соединения от браузеров

cd "$(dirname "$0")"

echo "🔗 Запуск Tunnel сервера (локальный прокси)..."
echo "   Принимает HTTP/SOCKS5 соединения на порту 8084"
echo "   Перенаправляет через VPN сервер на порту 8080"
echo ""
echo "📋 Настройки для браузера/приложений:"
echo "   HTTP Proxy: 127.0.0.1:8084"
echo "   SOCKS5 Proxy: 127.0.0.1:8084"
echo ""

# Создаем временный конфиг только для Tunnel сервера
cat > config_tunnel.json << 'EOF'
{
    "server": {
        "host": "127.0.0.1",
        "port": 8080,
        "max_connections": 100,
        "timeout": 30
    },
    "tunnel": {
        "host": "127.0.0.1",
        "port": 8084
    },
    "encryption": {
        "algorithm": "xor",
        "library": "./build/encryption_plugins/xor_encryption",
        "key": "42"
    },
    "server_mode": "proxy_only",
    "logging": {
        "level": "info",
        "file": "",
        "format": "timestamp"
    }
}
EOF

# Запускаем Tunnel сервер
./build/local-tunnel-server config_tunnel.json

# Удаляем временный конфиг
rm -f config_tunnel.json