# Custom VPN Server

VPN сервер (TCP прокси) для Linux на C++.

## Возможности

- TCP прокси сервер
- Поддержка нескольких клиентов одновременно
- Конфигурация через JSON файл
- Собственная система логирования
- Многопоточная архитектура

## Сборка

### Требования

- CMake 3.16+
- GCC/Clang с поддержкой C++17
- Linux

### Компиляция

```bash
./build.sh
```

Или вручную:

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Использование

### Запуск сервера

```bash
./run.sh
```

Или из директории build:

```bash
cd build
./custom-vpn
```

### Тестирование

Используйте тестовый клиент:

```bash
cd build
./test-client 127.0.0.1 8080 google.com:80
```

### Конфигурация

Отредактируйте файл `config.json`:

```json
{
    "server": {
        "host": "0.0.0.0",
        "port": 8080,
        "max_connections": 100,
        "buffer_size": 4096,
        "timeout": 30
    },
    "logging": {
        "level": "INFO",
        "file": "vpn_server.log"
    }
}
```

## Протокол

Клиент подключается к серверу и отправляет:
1. 4 байта - длина имени хоста (network byte order)
2. N байт - имя хоста
3. 2 байта - порт целевого сервера (network byte order)

Сервер отвечает:
- 1 байт - статус (1 = успех, 0 = ошибка)

После успешного установления соединения начинается прозрачная передача данных.

## Структура проекта

- `src/main.cpp` - Точка входа
- `src/vpn_server.cpp/.h` - Основной серверный класс
- `src/proxy_handler.cpp/.h` - Обработчик клиентских соединений
- `src/config.cpp/.h` - Управление конфигурацией
- `src/logger.cpp/.h` - Система логирования
- `src/utils.cpp/.h` - Вспомогательные функции
- `test_client.cpp` - Тестовый клиент
- `config.json` - Файл конфигурации
- `CMakeLists.txt` - Конфигурация сборки