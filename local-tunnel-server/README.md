# Local Tunnel Server

Многоплатформенный двухсокетный туннельный сервер с модульной системой шифрования.

## Возможности

- **Двухсокетная архитектура**: Proxy Server (8080) + Tunnel Server (8081)
- **Модульное шифрование**: Подключаемые алгоритмы через динамические библиотеки
- **Кроссплатформенность**: Поддержка Linux и Windows
- **Многопоточность**: Обработка нескольких клиентов одновременно
- **Гибкая конфигурация**: Настройка через JSON файл
- **Подробное логирование**: Отслеживание соединений и передачи данных

## Поддерживаемые алгоритмы шифрования

- **XOR шифрование** - простое и быстрое
- **Caesar шифр** - классическое смещение символов
- **Возможность добавления новых** алгоритмов через плагины

## Установка и сборка

### Linux

```bash
./build.sh
# или
mkdir build && cd build
cmake .. && make -j$(nproc)
```

### Windows

**Рекомендуется нативная сборка на Windows:**

```cmd
build_windows.bat
```

**Экспериментальная кросс-компиляция под Linux:**

```bash
./build_windows_cross.sh
```

Подробные инструкции: [BUILD_WINDOWS.md](BUILD_WINDOWS.md)

## Использование

### Запуск сервера (Linux)

```bash
./build/local-tunnel-server
```

### Запуск сервера (Windows)

```cmd
build\Release\local-tunnel-server.exe
```

### Конфигурация

Основные настройки в `config.json`:

```json
{
    "encryption": {
        "library_path": "./build/encryption_plugins/libxor_encryption.so",
        "algorithm": "XOR",
        "key": "MySecretKey123"
    }
}
```

Для Windows используйте `config_windows.json` с `.dll` плагинами.

## Структура проекта

- `vpn_server.py` - Основной серверный модуль
- `proxy_handler.py` - Обработчик прокси соединений
- `config.json` - Файл конфигурации
- `utils.py` - Вспомогательные функции
- `requirements.txt` - Зависимости проекта