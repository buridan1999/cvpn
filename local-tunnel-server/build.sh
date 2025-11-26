#!/bin/bash

# Скрипт для сборки проекта

echo "=== Сборка Local Tunnel Server ==="

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
    echo "Исполняемый файл: ./build/local-tunnel-server"
    echo ""
    echo "Для запуска выполните:"
    echo "  cd build"
    echo "  ./local-tunnel-server"
else
    echo "❌ Ошибка при сборке!"
    exit 1
fi