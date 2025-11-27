#!/bin/bash

# =============================================================================
# Универсальный скрипт сборки Local Tunnel Server
# Собирает сервер и плагины для Linux и Windows (кросс-компиляция)
# =============================================================================

set -e  # Остановка при ошибке

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Функция для вывода с цветом
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE} $1${NC}"
    echo -e "${BLUE}================================${NC}"
}

# Проверка зависимостей
check_dependencies() {
    print_header "Проверка зависимостей"
    
    # Проверка CMake
    if ! command -v cmake &> /dev/null; then
        print_error "CMake не найден! Установите cmake."
        exit 1
    fi
    
    # Проверка компилятора для Linux
    if ! command -v g++ &> /dev/null; then
        print_error "g++ не найден! Установите build-essential."
        exit 1
    fi
    
    # Проверка кросс-компилятора для Windows
    if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
        print_warning "MinGW-w64 кросс-компилятор не найден."
        print_warning "Сборка для Windows будет пропущена."
        print_warning "Для установки выполните: sudo apt install mingw-w64"
        WINDOWS_BUILD=false
    else
        WINDOWS_BUILD=true
    fi
    
    print_status "Проверка зависимостей завершена"
}

# Очистка старых сборок
clean_builds() {
    print_header "Очистка старых сборок"
    
    rm -rf build/ build_debug/ build_windows/ build_windows_debug/ dist/
    
    print_status "Очистка завершена"
}

# Создание структуры директорий для дистрибуции
create_dist_structure() {
    print_header "Создание структуры дистрибуции"
    
    mkdir -p dist/linux/{bin,encryption_plugins,configs}
    mkdir -p dist/windows/{bin,encryption_plugins,configs}
    mkdir -p dist/source
    
    print_status "Структура директорий создана"
}

# Сборка для Linux
build_linux() {
    print_header "Сборка для Linux"
    
    # Release версия
    print_status "Сборка Release версии для Linux..."
    mkdir -p build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j$(nproc)
    cd ..
    
    # Debug версия с AddressSanitizer
    print_status "Сборка Debug версии для Linux..."
    mkdir -p build_debug
    cd build_debug
    cmake -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_CXX_FLAGS="-g -O0 -Wall -Wextra -pedantic -fsanitize=address -fno-omit-frame-pointer" \
          ..
    make -j$(nproc)
    cd ..
    
    # Копирование файлов в дистрибуцию
    print_status "Копирование Linux файлов в дистрибуцию..."
    cp build/local-tunnel-server dist/linux/bin/
    cp build/test-client dist/linux/bin/
    cp build/encryption_plugins/*.so dist/linux/encryption_plugins/
    
    # Отладочная версия
    cp build_debug/local-tunnel-server dist/linux/bin/local-tunnel-server-debug
    
    print_status "Сборка для Linux завершена"
}

# Сборка для Windows (кросс-компиляция)
build_windows() {
    if [ "$WINDOWS_BUILD" = false ]; then
        print_warning "Сборка для Windows пропущена (нет MinGW-w64)"
        return
    fi
    
    print_header "Сборка для Windows (кросс-компиляция)"
    
    # Release версия
    print_status "Сборка Release версии для Windows..."
    mkdir -p build_windows
    cd build_windows
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw64-toolchain.cmake \
          -DCMAKE_BUILD_TYPE=Release \
          ..
    make -j$(nproc)
    cd ..
    
    # Debug версия
    print_status "Сборка Debug версии для Windows..."
    mkdir -p build_windows_debug
    cd build_windows_debug
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw64-toolchain.cmake \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_CXX_FLAGS="-g -O0 -Wall -Wextra -pedantic" \
          ..
    make -j$(nproc)
    cd ..
    
    # Копирование файлов в дистрибуцию
    print_status "Копирование Windows файлов в дистрибуцию..."
    cp build_windows/local-tunnel-server.exe dist/windows/bin/
    cp build_windows/test-client.exe dist/windows/bin/
    cp build_windows/encryption_plugins/*.dll dist/windows/encryption_plugins/
    
    # Отладочная версия
    cp build_windows_debug/local-tunnel-server.exe dist/windows/bin/local-tunnel-server-debug.exe
    
    print_status "Сборка для Windows завершена"
}

# Создание toolchain файла для Windows
create_windows_toolchain() {
    if [ "$WINDOWS_BUILD" = false ]; then
        return
    fi
    
    print_status "Создание CMake toolchain для Windows..."
    
    mkdir -p cmake
    cat > cmake/mingw64-toolchain.cmake << 'EOF'
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the cross compiler
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Where to look for the target environment
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Static linking for portability
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++")
EOF
}

# Копирование конфигурационных файлов
copy_configs() {
    print_header "Копирование конфигурационных файлов"
    
    # Linux конфиги
    cp config.json dist/linux/configs/
    cp config_proxy_only.json dist/linux/configs/
    cp config_tunnel_only.json dist/linux/configs/
    
    # Windows конфиги
    cp config_windows.json dist/windows/configs/config.json
    cp config_proxy_only.json dist/windows/configs/
    cp config_tunnel_only.json dist/windows/configs/
    
    # Исходный код
    print_status "Копирование исходного кода..."
    cp -r src dist/source/
    cp -r encryption_plugins dist/source/
    cp CMakeLists.txt dist/source/
    cp *.md dist/source/
    
    print_status "Конфигурационные файлы скопированы"
}

# Создание скриптов запуска
create_launch_scripts() {
    print_header "Создание скриптов запуска"
    
    # Linux скрипты
    cat > dist/linux/start_tunnel.sh << 'EOF'
#!/bin/bash
echo "Запуск Local Tunnel Server (режим туннель)"
./bin/local-tunnel-server configs/config_tunnel_only.json
EOF
    
    cat > dist/linux/start_proxy.sh << 'EOF'
#!/bin/bash
echo "Запуск Local Tunnel Server (режим прокси)"
./bin/local-tunnel-server configs/config_proxy_only.json
EOF
    
    cat > dist/linux/start_both.sh << 'EOF'
#!/bin/bash
echo "Запуск Local Tunnel Server (полный режим)"
./bin/local-tunnel-server configs/config.json
EOF
    
    chmod +x dist/linux/*.sh
    
    # Windows скрипты
    cat > dist/windows/start_tunnel.bat << 'EOF'
@echo off
echo Запуск Local Tunnel Server (режим туннель)
bin\local-tunnel-server.exe configs\config_tunnel_only.json
pause
EOF
    
    cat > dist/windows/start_proxy.bat << 'EOF'
@echo off
echo Запуск Local Tunnel Server (режим прокси)
bin\local-tunnel-server.exe configs\config_proxy_only.json
pause
EOF
    
    cat > dist/windows/start_both.bat << 'EOF'
@echo off
echo Запуск Local Tunnel Server (полный режим)
bin\local-tunnel-server.exe configs\config.json
pause
EOF
    
    print_status "Скрипты запуска созданы"
}

# Создание README файлов
create_readme() {
    print_header "Создание документации"
    
    cat > dist/README.md << 'EOF'
# Local Tunnel Server - Дистрибуция

Этот архив содержит скомпилированные версии Local Tunnel Server для Linux и Windows.

## Структура

- `linux/` - Версия для Linux
- `windows/` - Версия для Windows  
- `source/` - Исходный код

## Быстрый старт

### Linux
```bash
cd linux
./start_both.sh
```

### Windows
```cmd
cd windows
start_both.bat
```

## Режимы работы

1. **Полный режим** (`start_both`) - Запускает и туннель и прокси
2. **Только туннель** (`start_tunnel`) - Только туннельный сервер
3. **Только прокси** (`start_proxy`) - Только прокси сервер

## Настройка браузера

Настройте HTTP прокси в браузере:
- **Адрес**: 127.0.0.1
- **Порт**: 8081

## Порты

- **8080** - VPN Server (удаленный сервер)
- **8081** - Tunnel Server (локальный, для браузера)
EOF
    
    # Linux README
    cat > dist/linux/README.md << 'EOF'
# Local Tunnel Server - Linux Version

## Файлы

- `bin/local-tunnel-server` - Основное приложение
- `bin/local-tunnel-server-debug` - Отладочная версия  
- `bin/test-client` - Тестовый клиент
- `encryption_plugins/` - Плагины шифрования
- `configs/` - Конфигурационные файлы

## Запуск

```bash
# Полный режим
./start_both.sh

# Только туннель
./start_tunnel.sh

# Только прокси  
./start_proxy.sh

# Ручной запуск
./bin/local-tunnel-server configs/config.json
```
EOF
    
    # Windows README
    cat > dist/windows/README.md << 'EOF'
# Local Tunnel Server - Windows Version

## Файлы

- `bin\local-tunnel-server.exe` - Основное приложение
- `bin\local-tunnel-server-debug.exe` - Отладочная версия
- `bin\test-client.exe` - Тестовый клиент  
- `encryption_plugins\` - Плагины шифрования
- `configs\` - Конфигурационные файлы

## Запуск

```cmd
# Полный режим
start_both.bat

# Только туннель
start_tunnel.bat

# Только прокси
start_proxy.bat

# Ручной запуск
bin\local-tunnel-server.exe configs\config.json
```
EOF
    
    print_status "Документация создана"
}

# Вывод итоговой информации
print_summary() {
    print_header "Сборка завершена!"
    
    echo -e "${GREEN}Результаты сборки:${NC}"
    echo "📁 dist/linux/     - Linux версия"
    echo "📁 dist/windows/   - Windows версия (кросс-компиляция)"
    echo "📁 dist/source/    - Исходный код"
    
    echo ""
    echo -e "${BLUE}Размеры:${NC}"
    if [ -d "dist/linux" ]; then
        echo "🐧 Linux:   $(du -sh dist/linux | cut -f1)"
    fi
    if [ -d "dist/windows" ]; then
        echo "🪟 Windows: $(du -sh dist/windows | cut -f1)"
    fi
    echo "📦 Всего:   $(du -sh dist | cut -f1)"
    
    echo ""
    echo -e "${GREEN}Для тестирования:${NC}"
    echo "cd dist/linux && ./start_both.sh"
    if [ "$WINDOWS_BUILD" = true ]; then
        echo "cd dist/windows && wine bin/local-tunnel-server.exe configs/config.json"
    fi
    
    echo ""
    echo -e "${YELLOW}Настройка прокси в браузере: 127.0.0.1:8081${NC}"
}

# Главная функция
main() {
    print_header "Универсальная сборка Local Tunnel Server"
    
    # Определение переменных
    WINDOWS_BUILD=true
    
    # Выполнение этапов
    check_dependencies
    clean_builds
    create_dist_structure
    create_windows_toolchain
    
    build_linux
    build_windows
    
    copy_configs
    create_launch_scripts
    create_readme
    
    print_summary
}

# Запуск с обработкой ошибок
if ! main "$@"; then
    print_error "Сборка завершилась с ошибкой!"
    exit 1
fi