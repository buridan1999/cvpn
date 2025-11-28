#!/bin/bash

# Упрощенная кросс-компиляция только плагинов для Windows
# Главная программа требует слишком много исправлений для MinGW

set -e

echo "=== Кросс-компиляция плагинов для Windows ==="

# Проверка зависимостей
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "❌ MinGW-w64 не найден"
    echo "Установите: sudo apt-get install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64"
    exit 1
fi

BUILD_DIR="build_windows_simple"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/encryption_plugins"

echo "✓ Компиляция XOR плагина..."
x86_64-w64-mingw32-g++ \
    -shared \
    -std=c++17 \
    -O3 \
    -DBUILDING_DLL \
    -static-libgcc \
    -static-libstdc++ \
    -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic \
    -o "$BUILD_DIR/encryption_plugins/xor_encryption.dll" \
    encryption_plugins/xor_encryption.cpp

echo "✓ Компиляция Caesar плагина..."
x86_64-w64-mingw32-g++ \
    -shared \
    -std=c++17 \
    -O3 \
    -DBUILDING_DLL \
    -static-libgcc \
    -static-libstdc++ \
    -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic \
    -o "$BUILD_DIR/encryption_plugins/caesar_encryption.dll" \
    encryption_plugins/caesar_encryption.cpp

echo ""
echo "=== Результаты ==="

if [ -f "$BUILD_DIR/encryption_plugins/xor_encryption.dll" ]; then
    echo "✓ XOR плагин: $(du -h $BUILD_DIR/encryption_plugins/xor_encryption.dll | cut -f1)"
    echo "  Формат: $(file $BUILD_DIR/encryption_plugins/xor_encryption.dll)"
    echo "  Зависимости:"
    x86_64-w64-mingw32-objdump -p "$BUILD_DIR/encryption_plugins/xor_encryption.dll" | grep -E 'DLL Name:' | head -5
fi

if [ -f "$BUILD_DIR/encryption_plugins/caesar_encryption.dll" ]; then
    echo "✓ Caesar плагин: $(du -h $BUILD_DIR/encryption_plugins/caesar_encryption.dll | cut -f1)"
fi

echo ""
echo "=== Для главной программы ==="
echo "Рекомендация: Используйте нативную Windows сборку для основной программы"
echo "Причина: MinGW имеет проблемы совместимости с std::thread и std::mutex"
echo "Альтернативы:"
echo "1. GitHub Actions с Windows runner"
echo "2. Docker с Windows контейнером"
echo "3. WSL для разработки + нативная сборка Windows"
echo "4. Visual Studio Community для Windows"

cd ..
echo ""
echo "Плагины готовы для Windows в папке: $BUILD_DIR"