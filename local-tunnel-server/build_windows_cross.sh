#!/bin/bash

echo "================================"
echo " Windows Cross-Compilation Build"
echo "================================"
echo

# Проверка установки MinGW-w64
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "ОШИБКА: MinGW-w64 не установлен!"
    echo
    echo "Установите на Ubuntu/Debian:"
    echo "  sudo apt update"
    echo "  sudo apt install mingw-w64 cmake"
    echo
    echo "Установите на Fedora:"
    echo "  sudo dnf install mingw64-gcc-c++ mingw64-winpthreads-static cmake"
    echo
    echo "Установите на Arch Linux:"
    echo "  sudo pacman -S mingw-w64-gcc cmake"
    exit 1
fi

# Создание директории для Windows сборки
BUILD_DIR="build-windows"
if [ -d "$BUILD_DIR" ]; then
    echo "Очистка предыдущей сборки..."
    rm -rf "$BUILD_DIR"
fi

mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Настройка кросс-компиляции для Windows..."

# Генерация проекта с кросс-компиляцией
cmake \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 \
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
    -DCMAKE_BUILD_TYPE=Release \
    ..

if [ $? -ne 0 ]; then
    echo "ОШИБКА: Не удалось сконфигурировать проект!"
    exit 1
fi

echo
echo "Начинаем сборку Windows версии..."

# Сборка
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo
    echo "ОШИБКА СБОРКИ!"
    exit 1
fi

echo
echo "================================"
echo " КРОСС-КОМПИЛЯЦИЯ ЗАВЕРШЕНА!"
echo "================================"
echo
echo "Созданные Windows файлы:"
if [ -f "local-tunnel-server.exe" ]; then
    echo "✓ local-tunnel-server.exe"
    file local-tunnel-server.exe
fi

if [ -f "test-client.exe" ]; then
    echo "✓ test-client.exe" 
    file test-client.exe
fi

if [ -f "encryption_plugins/libxor_encryption.dll" ]; then
    echo "✓ encryption_plugins/libxor_encryption.dll"
    file encryption_plugins/libxor_encryption.dll
fi

if [ -f "encryption_plugins/libcaesar_encryption.dll" ]; then
    echo "✓ encryption_plugins/libcaesar_encryption.dll"
    file encryption_plugins/libcaesar_encryption.dll
fi

echo
echo "Размеры файлов:"
ls -lh *.exe encryption_plugins/*.dll 2>/dev/null || echo "Некоторые файлы не найдены"

echo
echo "Проверка зависимостей:"
echo "local-tunnel-server.exe:"
x86_64-w64-mingw32-objdump -p local-tunnel-server.exe | grep "DLL Name:" | head -10

echo
echo "Файлы готовы для тестирования на Windows!"
echo "Скопируйте содержимое папки build-windows/ на Windows машину."