#!/bin/bash

# Скрипт для запуска VPN сервера

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
CONFIG_FILE="$SCRIPT_DIR/config.json"

echo "=== Custom VPN Server Launcher ==="

# Проверка наличия собранного проекта
if [ ! -f "$BUILD_DIR/custom-vpn" ]; then
    echo "❌ Исполняемый файл не найден!"
    echo "Запустите сборку: ./build.sh"
    exit 1
fi

# Копирование конфигурации в директорию сборки
if [ -f "$CONFIG_FILE" ]; then
    cp "$CONFIG_FILE" "$BUILD_DIR/"
    echo "📋 Конфигурация скопирована"
fi

# Переход в директорию сборки и запуск
cd "$BUILD_DIR"

echo "🚀 Запуск VPN сервера..."
echo "Для остановки нажмите Ctrl+C"
echo ""

./custom-vpn