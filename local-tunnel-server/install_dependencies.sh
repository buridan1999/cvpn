#!/bin/bash

# =============================================================================
# Установка зависимостей для Local Tunnel Server
# =============================================================================

set -e

# Цвета
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Определение дистрибутива
detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$NAME
        VER=$VERSION_ID
    elif [ -f /etc/debian_version ]; then
        OS="Debian"
        VER=$(cat /etc/debian_version)
    else
        OS=$(uname -s)
        VER=$(uname -r)
    fi
    
    print_status "Определена ОС: $OS $VER"
}

# Установка для Ubuntu/Debian
install_ubuntu_debian() {
    print_status "Установка зависимостей для Ubuntu/Debian..."
    
    sudo apt update
    
    # Основные пакеты
    sudo apt install -y \
        build-essential \
        cmake \
        git \
        pkg-config \
        libssl-dev \
        libcurl4-openssl-dev
    
    # MinGW-w64 для кросс-компиляции Windows
    print_status "Установка MinGW-w64 для кросс-компиляции Windows..."
    sudo apt install -y \
        mingw-w64 \
        mingw-w64-tools
    
    print_status "Установка завершена для Ubuntu/Debian"
}

# Установка для CentOS/RHEL/Fedora
install_centos_rhel_fedora() {
    if command -v dnf &> /dev/null; then
        PKG_MANAGER="dnf"
    else
        PKG_MANAGER="yum"
    fi
    
    print_status "Установка зависимостей для CentOS/RHEL/Fedora..."
    
    # Основные пакеты
    sudo $PKG_MANAGER install -y \
        gcc-c++ \
        cmake \
        git \
        openssl-devel \
        libcurl-devel
    
    # MinGW для кросс-компиляции (если доступен)
    if $PKG_MANAGER list mingw64-gcc-c++ &>/dev/null; then
        print_status "Установка MinGW-w64..."
        sudo $PKG_MANAGER install -y \
            mingw64-gcc-c++ \
            mingw64-winpthreads-static
    else
        print_warning "MinGW-w64 недоступен в репозиториях"
        print_warning "Сборка для Windows будет недоступна"
    fi
    
    print_status "Установка завершена для CentOS/RHEL/Fedora"
}

# Установка для Arch Linux
install_arch() {
    print_status "Установка зависимостей для Arch Linux..."
    
    # Основные пакеты
    sudo pacman -S --noconfirm \
        base-devel \
        cmake \
        git \
        openssl \
        curl
    
    # MinGW для кросс-компиляции
    print_status "Установка MinGW-w64..."
    sudo pacman -S --noconfirm \
        mingw-w64-gcc
    
    print_status "Установка завершена для Arch Linux"
}

# Проверка установленных зависимостей
check_dependencies() {
    print_status "Проверка установленных зависимостей..."
    
    DEPS_OK=true
    
    # Проверка основных утилит
    for cmd in gcc g++ cmake make git; do
        if command -v $cmd &> /dev/null; then
            echo "✅ $cmd: $(command -v $cmd)"
        else
            echo "❌ $cmd: не найден"
            DEPS_OK=false
        fi
    done
    
    # Проверка MinGW
    if command -v x86_64-w64-mingw32-g++ &> /dev/null; then
        echo "✅ MinGW-w64: $(command -v x86_64-w64-mingw32-g++)"
        echo "   🪟 Сборка для Windows доступна"
    else
        echo "⚠️  MinGW-w64: не найден"
        echo "   🪟 Сборка для Windows недоступна"
    fi
    
    if [ "$DEPS_OK" = true ]; then
        print_status "Все основные зависимости установлены!"
    else
        print_error "Некоторые зависимости отсутствуют"
        return 1
    fi
}

# Показ следующих шагов
show_next_steps() {
    echo ""
    echo "🎉 Установка зависимостей завершена!"
    echo ""
    echo "Следующие шаги:"
    echo "1. Клонируйте репозиторий (если еще не сделали):"
    echo "   git clone <repository-url>"
    echo ""
    echo "2. Перейдите в директорию проекта:"
    echo "   cd local-tunnel-server"
    echo ""
    echo "3. Выполните сборку:"
    echo "   ./build_quick.sh      # Быстрая сборка для Linux"
    echo "   ./build_all.sh        # Полная сборка (Linux + Windows)"
    echo ""
    echo "4. Запустите сервер:"
    echo "   cd build && ./local-tunnel-server ../config.json"
    echo ""
    echo "5. Настройте прокси в браузере: 127.0.0.1:8081"
}

# Главная функция
main() {
    echo "🔧 Установка зависимостей для Local Tunnel Server"
    echo "=================================================="
    
    detect_os
    
    case $OS in
        *Ubuntu*|*Debian*)
            install_ubuntu_debian
            ;;
        *CentOS*|*"Red Hat"*|*Fedora*)
            install_centos_rhel_fedora
            ;;
        *Arch*)
            install_arch
            ;;
        *)
            print_warning "Неизвестный дистрибутив: $OS"
            print_warning "Попробуйте установить зависимости вручную:"
            echo "- build-essential (gcc, g++, make)"
            echo "- cmake"
            echo "- git"
            echo "- mingw-w64 (для кросс-компиляции Windows)"
            exit 1
            ;;
    esac
    
    check_dependencies
    show_next_steps
}

main "$@"