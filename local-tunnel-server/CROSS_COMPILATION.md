# Кросс-компиляция Windows версии под Linux

## Статус: ЭКСПЕРИМЕНТАЛЬНО

Кросс-компиляция Windows DLL и исполняемых файлов под Linux возможна с использованием MinGW-w64, но находится в экспериментальном состоянии.

## Что работает

✅ **Динамические библиотеки плагинов шифрования**
- XOR и Caesar шифрование компилируются в .dll файлы
- Правильная настройка экспорта символов для Windows
- CMake автоматически определяет платформу и настраивает сборку

✅ **Кросс-платформенная совместимость**
- Единая система сборки для Linux (.so) и Windows (.dll)
- Автоматическое определение целевой платформы
- Правильные пути к библиотекам в конфигурации

✅ **Базовая функциональность**
- Основные структуры проекта компилируются
- Плагинная архитектура работает корректно

## Текущие ограничения

❌ **Проблемы с заголовками**
- Конфликты между Unix и Windows заголовками
- Проблемы с threading под MinGW-w64
- Несовместимость некоторых системных вызовов

❌ **Сложности с сокетами**
- Windows Sockets (Winsock2) vs Berkeley sockets
- Различия в API функций (inet_pton, setsockopt)
- Проблемы с типами данных

## Использование

### Быстрый старт

```bash
# Установка MinGW-w64
sudo apt install mingw-w64 cmake

# Кросс-компиляция
./build_windows_cross.sh
```

### Docker сборка

```bash
# Сборка образа
docker build -f Dockerfile.windows -t local-tunnel-windows .

# Извлечение файлов
docker run --rm -v $(pwd)/output:/output local-tunnel-windows \
  sh -c "cp -r build-windows/* /output/"
```

### Результат

При успешной компиляции в `build-windows/` появятся:
- `encryption_plugins/libxor_encryption.dll`
- `encryption_plugins/libcaesar_encryption.dll`

## Рекомендации

### Для разработки
Используйте **нативную сборку на Windows** с Visual Studio или MinGW-w64.

### Для автоматизации CI/CD
1. **GitHub Actions**: Используйте `windows-latest` runner
2. **Docker**: Используйте Windows контейнеры
3. **Cross-compilation**: Только для быстрого тестирования

### Пример конфигурации для Windows

```json
{
    "encryption": {
        "library_path": ".\\\\build\\\\encryption_plugins\\\\xor_encryption.dll",
        "algorithm": "XOR",
        "key": "MySecretKey123"
    }
}
```

## Техническая информация

### Что было реализовано

1. **CMakeLists.txt обновлен** для условной компиляции
2. **platform_compat.h** создан для кросс-платформенных заголовков  
3. **encryption_export.h** настроен для Windows DLL экспорта
4. **Плагины шифрования** обновлены для Windows совместимости

### Архитектурные решения

```cpp
// Пример кросс-платформенного кода
#ifdef _WIN32
    #include <winsock2.h>
    #define close(fd) closesocket(fd)
#else
    #include <sys/socket.h>
    #include <unistd.h>
#endif
```

## Альтернативы

1. **Нативная сборка Windows**: Рекомендуется для продакшена
2. **MSYS2/MinGW-w64 на Windows**: Стабильная среда сборки
3. **Visual Studio**: Лучшая IDE для Windows разработки
4. **WSL**: Windows Subsystem for Linux для разработки

## Заключение

Кросс-компиляция полезна для:
- ✅ Быстрого тестирования совместимости
- ✅ Автоматизированной сборки плагинов
- ✅ CI/CD пайплайнов

НО не рекомендуется для:
- ❌ Продакшен сборок
- ❌ Отладки Windows-специфических проблем
- ❌ Сложных Windows приложений

**Используйте нативную сборку на Windows для стабильных результатов.**