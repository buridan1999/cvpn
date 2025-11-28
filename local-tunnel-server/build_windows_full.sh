#!/bin/bash

# Полная кросс-компиляция для Windows под Linux
# Включает основную программу и плагины шифрования

set -e  # Выход при ошибке

echo "=== Полная кросс-компиляция для Windows ==="

# Проверка зависимостей
echo "Проверка зависимостей MinGW-w64..."
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "❌ MinGW-w64 не найден"
    echo "Установите: sudo apt-get install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64"
    exit 1
fi

if ! command -v cmake &> /dev/null; then
    echo "❌ CMake не найден"
    echo "Установите: sudo apt-get install cmake"
    exit 1
fi

echo "✓ MinGW-w64: $(x86_64-w64-mingw32-gcc --version | head -n1)"
echo "✓ CMake: $(cmake --version | head -n1)"

# Создание директории для сборки
BUILD_DIR="build_windows"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo "Настройка CMake для кросс-компиляции..."

# Создание toolchain файла для CMake
cat > "$BUILD_DIR/windows_toolchain.cmake" << 'EOF'
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 1)

# Компилятор
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Пути поиска библиотек
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Специфичные для Windows настройки
set(WIN32 TRUE)
set(MINGW TRUE)

# Дополнительные флаги компиляции для совместимости
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 -pthread")

# Статическая линковка для портативности
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -pthread")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++ -pthread")
EOF

cd "$BUILD_DIR"

echo "Конфигурирование проекта..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=windows_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=windows_install \
    -DENABLE_DEBUG_LOGGING=OFF

echo "Компиляция..."
make -j$(nproc)

echo ""
echo "=== Результаты компиляции ==="

# Проверка результатов
if [ -f "local-tunnel-server.exe" ]; then
    echo "✓ Основная программа: local-tunnel-server.exe"
    echo "  Размер: $(du -h local-tunnel-server.exe | cut -f1)"
    echo "  Формат: $(file local-tunnel-server.exe)"
else
    echo "❌ Основная программа не создана"
fi

if [ -f "test-client.exe" ]; then
    echo "✓ Тестовый клиент: test-client.exe"
    echo "  Размер: $(du -h test-client.exe | cut -f1)"
else
    echo "❌ Тестовый клиент не создан"
fi

# Проверка плагинов шифрования
echo ""
echo "Плагины шифрования:"
if [ -f "encryption_plugins/xor_encryption.dll" ]; then
    echo "✓ XOR шифрование: encryption_plugins/xor_encryption.dll"
    echo "  Размер: $(du -h encryption_plugins/xor_encryption.dll | cut -f1)"
    echo "  Зависимости: $(x86_64-w64-mingw32-objdump -p encryption_plugins/xor_encryption.dll | grep -i dll | head -5)"
else
    echo "❌ XOR плагин не создан"
fi

if [ -f "encryption_plugins/caesar_encryption.dll" ]; then
    echo "✓ Caesar шифрование: encryption_plugins/caesar_encryption.dll"
    echo "  Размер: $(du -h encryption_plugins/caesar_encryption.dll | cut -f1)"
else
    echo "❌ Caesar плагин не создан"
fi

echo ""
echo "=== Проверка зависимостей ==="

if [ -f "local-tunnel-server.exe" ]; then
    echo "Зависимости основной программы:"
    x86_64-w64-mingw32-objdump -p local-tunnel-server.exe | grep -E 'DLL Name:' | head -10 || echo "Нет внешних зависимостей"
fi

echo ""
echo "=== Инструкции для использования ==="
echo "1. Скопируйте файлы на Windows:"
echo "   - local-tunnel-server.exe"
echo "   - test-client.exe"
echo "   - encryption_plugins/*.dll"
echo "   - config.json"
echo ""
echo "2. Запуск на Windows:"
echo "   local-tunnel-server.exe [config.json]"
echo ""
echo "3. Тестирование:"
echo "   test-client.exe"

cd ..
echo ""
echo "=== Сборка завершена ==="
echo "Результаты находятся в папке: $BUILD_DIR"