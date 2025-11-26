#!/bin/bash

# Скрипт для сборки проекта

echo "=== Сборка Custom VPN Server ==="

# Создание директории сборки
mkdir -p build
cd build

# Настройка и сборка
echo "Настройка проекта..."
cmake .. -DCMAKE_BUILD_TYPE=Release

echo "Компиляция..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo "✅ Сборка успешно завершена!"
    echo "Исполняемый файл: ./build/custom-vpn"
    echo ""
    echo "Для запуска выполните:"
    echo "  cd build"
    echo "  ./custom-vpn"
else
    echo "❌ Ошибка при сборке!"
    exit 1
fi