# Создание плагинов шифрования

Данное руководство объясняет, как создать собственные алгоритмы шифрования для Local Tunnel Server.

## Интерфейс IEncryption

Все алгоритмы должны наследоваться от базового интерфейса:

```cpp
class IEncryption {
public:
    virtual ~IEncryption() = default;
    
    // Инициализация с ключом
    virtual bool init(const unsigned char* key, size_t key_size) = 0;
    
    // Шифрование данных
    virtual void encrypt(unsigned char* data, size_t size) = 0;
    
    // Дешифрование данных  
    virtual void decrypt(unsigned char* data, size_t size) = 0;
    
    // Имя алгоритма
    virtual const char* get_name() const = 0;
    
    // Версия алгоритма
    virtual const char* get_version() const = 0;
};
```

## Пример плагина: ROT13 шифрование

### rot13_encryption.cpp

```cpp
#include "../src/encryption_interface.h"
#include "encryption_export.h"
#include <cstring>

class ROT13Encryption : public IEncryption {
private:
    bool initialized_;
    
public:
    ROT13Encryption() : initialized_(false) {}
    
    bool init(const unsigned char* key, size_t key_size) override {
        // ROT13 не требует ключа, но мы должны вызвать init()
        initialized_ = true;
        return true;
    }
    
    void encrypt(unsigned char* data, size_t size) override {
        if (!initialized_) return;
        
        for (size_t i = 0; i < size; ++i) {
            if (data[i] >= 'A' && data[i] <= 'Z') {
                data[i] = 'A' + (data[i] - 'A' + 13) % 26;
            } else if (data[i] >= 'a' && data[i] <= 'z') {
                data[i] = 'a' + (data[i] - 'a' + 13) % 26;
            }
        }
    }
    
    void decrypt(unsigned char* data, size_t size) override {
        // ROT13 обратим сам к себе
        encrypt(data, size);
    }
    
    const char* get_name() const override {
        return "ROT13";
    }
    
    const char* get_version() const override {
        return "1.0.0";
    }
};

// Экспортируемые функции
extern "C" {
    ENCRYPTION_API IEncryption* ENCRYPTION_CALL create_encryption() {
        return new ROT13Encryption();
    }

    ENCRYPTION_API void ENCRYPTION_CALL destroy_encryption(IEncryption* encryption) {
        delete encryption;
    }
}
```

## Добавление в CMakeLists.txt

```cmake
# Добавьте после существующих плагинов
add_library(rot13_encryption SHARED encryption_plugins/rot13_encryption.cpp)

if(WIN32)
    set_target_properties(rot13_encryption PROPERTIES
        PREFIX ""
        OUTPUT_NAME "rot13_encryption"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/encryption_plugins"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/encryption_plugins"
    )
    target_compile_definitions(rot13_encryption PRIVATE BUILDING_DLL)
else()
    set_target_properties(rot13_encryption PROPERTIES
        PREFIX "lib"
        OUTPUT_NAME "rot13_encryption"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/encryption_plugins"
    )
endif()
```

## Конфигурация

### Linux
```json
{
    "encryption": {
        "library_path": "./build/encryption_plugins/librot13_encryption.so",
        "algorithm": "ROT13",
        "key": "unused"
    }
}
```

### Windows
```json
{
    "encryption": {
        "library_path": ".\\build\\encryption_plugins\\rot13_encryption.dll",
        "algorithm": "ROT13", 
        "key": "unused"
    }
}
```

## Требования к плагинам

### Обязательные функции
- `create_encryption()` - создание экземпляра
- `destroy_encryption()` - освобождение памяти

### Экспорт символов
- **Linux**: Использует `__attribute__((visibility("default")))`
- **Windows**: Использует `__declspec(dllexport)` при компиляции DLL

### Обработка ошибок
- `init()` должен возвращать `false` при ошибке инициализации
- Методы `encrypt()`/`decrypt()` должны проверять `initialized_` состояние
- Никогда не бросайте исключения из экспортируемых функций

## Продвинутые возможности

### Состояние шифрования
```cpp
class StreamCipher : public IEncryption {
private:
    std::vector<unsigned char> key_stream_;
    size_t position_;
    
public:
    bool init(const unsigned char* key, size_t key_size) override {
        // Генерация потока ключей на основе входного ключа
        key_stream_.resize(1024);
        // ... инициализация алгоритма
        position_ = 0;
        return true;
    }
    
    void encrypt(unsigned char* data, size_t size) override {
        for (size_t i = 0; i < size; ++i) {
            data[i] ^= key_stream_[position_ % key_stream_.size()];
            position_++;
        }
    }
};
```

### Блочное шифрование
```cpp
class BlockCipher : public IEncryption {
private:
    static const size_t BLOCK_SIZE = 16;
    unsigned char key_[BLOCK_SIZE];
    
public:
    void encrypt(unsigned char* data, size_t size) override {
        // Обрабатываем блоки по BLOCK_SIZE байт
        for (size_t i = 0; i < size; i += BLOCK_SIZE) {
            size_t block_size = std::min(BLOCK_SIZE, size - i);
            encrypt_block(data + i, block_size);
        }
    }
    
private:
    void encrypt_block(unsigned char* block, size_t size) {
        // Реализация блочного алгоритма
    }
};
```

## Отладка плагинов

### Linux
```bash
# Проверка символов в библиотеке
nm -D ./build/encryption_plugins/librot13_encryption.so | grep create

# Тестирование загрузки
ldd ./build/encryption_plugins/librot13_encryption.so
```

### Windows  
```cmd
# Проверка экспортируемых функций
dumpbin /exports encryption_plugins\rot13_encryption.dll
```

### Логирование
Используйте стандартный вывод для отладочной информации:
```cpp
void encrypt(unsigned char* data, size_t size) override {
    std::cerr << "ROT13: encrypting " << size << " bytes" << std::endl;
    // ... реализация
}
```

## Производительность

### Оптимизации
- Минимизируйте выделение памяти в `encrypt()`/`decrypt()`
- Используйте встроенные оптимизации компилятора
- Для Windows добавьте `/O2` флаг оптимизации

### Профилирование
```cpp
#include <chrono>

void encrypt(unsigned char* data, size_t size) override {
    auto start = std::chrono::high_resolution_clock::now();
    // ... шифрование
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cerr << "Encryption took: " << duration.count() << "μs" << std::endl;
}
```