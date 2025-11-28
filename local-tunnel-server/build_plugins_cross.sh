#!/bin/bash

echo "================================"
echo " Windows Plugins Cross-Compilation"
echo "================================"
echo

# Проверка установки MinGW-w64
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "ОШИБКА: MinGW-w64 не установлен!"
    echo
    echo "Установите на Ubuntu/Debian:"
    echo "  sudo apt update"
    echo "  sudo apt install mingw-w64 cmake"
    exit 1
fi

# Создание директории для Windows сборки плагинов
BUILD_DIR="build-windows-plugins"
if [ -d "$BUILD_DIR" ]; then
    echo "Очистка предыдущей сборки..."
    rm -rf "$BUILD_DIR"
fi

mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Настройка кросс-компиляции плагинов для Windows..."

# Создаем минимальный CMakeLists.txt только для плагинов
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.16)
project(EncryptionPlugins VERSION 1.0.0 LANGUAGES CXX)

# Стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Создание динамических библиотек шифрования
add_library(xor_encryption SHARED ../encryption_plugins/xor_encryption.cpp)
add_library(caesar_encryption SHARED ../encryption_plugins/caesar_encryption.cpp)

# Настройки для Windows DLL
set_target_properties(xor_encryption PROPERTIES
    PREFIX ""
    OUTPUT_NAME "xor_encryption"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/encryption_plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/encryption_plugins"
    SUFFIX ".dll"
)

set_target_properties(caesar_encryption PROPERTIES
    PREFIX ""
    OUTPUT_NAME "caesar_encryption"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/encryption_plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/encryption_plugins"
    SUFFIX ".dll"
)

# Экспорт символов для Windows DLL
target_compile_definitions(xor_encryption PRIVATE BUILDING_DLL)
target_compile_definitions(caesar_encryption PRIVATE BUILDING_DLL)

# Статическая линковка для портативности
target_link_libraries(xor_encryption -static-libgcc -static-libstdc++)
target_link_libraries(caesar_encryption -static-libgcc -static-libstdc++)
EOF

# Генерация проекта с кросс-компиляцией
cmake \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_BUILD_TYPE=Release \
    .

if [ $? -ne 0 ]; then
    echo "ОШИБКА: Не удалось сконфигурировать проект!"
    exit 1
fi

echo
echo "Начинаем сборку Windows плагинов..."

# Сборка
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo
    echo "ОШИБКА СБОРКИ!"
    exit 1
fi

echo
echo "================================"
echo " СБОРКА ПЛАГИНОВ ЗАВЕРШЕНА!"
echo "================================"
echo
echo "Созданные Windows файлы:"

if [ -f "encryption_plugins/xor_encryption.dll" ]; then
    echo "✓ encryption_plugins/xor_encryption.dll"
    file encryption_plugins/xor_encryption.dll
fi

if [ -f "encryption_plugins/caesar_encryption.dll" ]; then
    echo "✓ encryption_plugins/caesar_encryption.dll"
    file encryption_plugins/caesar_encryption.dll
fi

echo
echo "Размеры файлов:"
ls -lh encryption_plugins/*.dll 2>/dev/null || echo "Файлы не найдены"

echo
echo "Проверка зависимостей DLL:"
x86_64-w64-mingw32-objdump -p encryption_plugins/xor_encryption.dll | grep "DLL Name:" | head -5

echo
echo "Плагины готовы для Windows!"
echo "Скопируйте содержимое папки encryption_plugins/ на Windows машину."