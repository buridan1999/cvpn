# Сборка Local Tunnel Server для Windows

## Требования

- **Visual Studio 2019/2022** или **MinGW-w64**
- **CMake 3.16+**
- **Git**

## Кросс-компиляция под Linux (ЭКСПЕРИМЕНТАЛЬНО)

### Установка MinGW-w64

```bash
# Ubuntu/Debian
sudo apt install mingw-w64 cmake

# Fedora
sudo dnf install mingw64-gcc-c++ mingw64-winpthreads-static cmake

# Arch Linux
sudo pacman -S mingw-w64-gcc cmake
```

### Кросс-компиляция

```bash
./build_windows_cross.sh
```

**ВНИМАНИЕ**: Кросс-компиляция находится в экспериментальной стадии. Некоторые библиотеки Windows могут компилироваться некорректно. Для стабильных результатов рекомендуется нативная сборка на Windows.

## Сборка с Visual Studio (РЕКОМЕНДУЕТСЯ)

### 1. Подготовка среды

```cmd
# Клонирование репозитория
git clone https://github.com/buridan1999/cvpn.git
cd cvpn/local-tunnel-server

# Создание директории сборки
mkdir build
cd build
```

### 2. Генерация проекта Visual Studio

```cmd
# Для Visual Studio 2022
cmake -G "Visual Studio 17 2022" -A x64 ..

# Для Visual Studio 2019
cmake -G "Visual Studio 16 2019" -A x64 ..
```

### 3. Сборка

```cmd
# Сборка в Release режиме
cmake --build . --config Release

# Или сборка в Debug режиме
cmake --build . --config Debug
```

## Сборка с MinGW (Нативно на Windows)

### 1. Установка MinGW-w64

Скачайте и установите MinGW-w64 или используйте MSYS2:

```cmd
# Через MSYS2
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-make
```

### 2. Сборка

```cmd
mkdir build
cd build

# Генерация Makefile
cmake -G "MinGW Makefiles" ..

# Сборка
mingw32-make -j%NUMBER_OF_PROCESSORS%
```

## Результат сборки

После успешной сборки в директории `build` появятся:

### Исполняемые файлы:
- `local-tunnel-server.exe` - основной сервер
- `test-client.exe` - тестовый клиент

### Плагины шифрования:
- `encryption_plugins/xor_encryption.dll` - XOR шифрование
- `encryption_plugins/caesar_encryption.dll` - Caesar шифрование

## Конфигурация

Используйте `config_windows.json` как пример конфигурации для Windows:

```json
{
    "encryption": {
        "library_path": ".\\build\\encryption_plugins\\xor_encryption.dll",
        "algorithm": "XOR",
        "key": "MySecretKey123"
    }
}
```

## Запуск

```cmd
# Из корневой директории проекта
local-tunnel-server.exe

# Или с указанием конфигурации
local-tunnel-server.exe config_windows.json
```

## Особенности Windows

1. **Пути к файлам**: Используйте обратные слэши `\\` или прямые `/`
2. **DLL файлы**: Плагины имеют расширение `.dll` вместо `.so`
3. **Winsock2**: Автоматически подключается `ws2_32.lib` для работы с сокетами
4. **Экспорт символов**: Используется `__declspec(dllexport)` для DLL

## Отладка

### Visual Studio:
1. Откройте проект в Visual Studio
2. Установите breakpoint в нужном месте
3. Запустите с отладкой (F5)

### MinGW + GDB:
```cmd
gdb local-tunnel-server.exe
(gdb) run
```

## Возможные проблемы

### Ошибка "DLL not found"
- Убедитесь, что пути к DLL указаны правильно
- Проверьте, что DLL находятся в той же директории или в PATH

### Ошибки кросс-компиляции
- Кросс-компиляция может вызывать проблемы с заголовками Windows
- Рекомендуется использовать нативную сборку на Windows
- Для продакшена используйте Visual Studio или MinGW на Windows

### Проблемы с сетью
- Проверьте настройки Windows Firewall
- Убедитесь, что порты 8080 и 8081 не заняты

## Альтернативные методы сборки

### CMake GUI:
1. Запустите cmake-gui
2. Укажите source и build директории
3. Нажмите "Configure" и выберите генератор
4. Нажмите "Generate"
5. Откройте сгенерированный проект

### Batch скрипт сборки:
```cmd
@echo off
mkdir build 2>nul
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
pause
```

## Рекомендации

**Для разработки**: Используйте Visual Studio на Windows
**Для автоматизации**: MinGW-w64 на Windows или Docker с Windows образом
**Кросс-компиляция**: Только для тестирования, не для продакшена