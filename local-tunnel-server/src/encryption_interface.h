#ifndef ENCRYPTION_INTERFACE_H
#define ENCRYPTION_INTERFACE_H

#include <cstddef>

/**
 * Интерфейс для алгоритмов шифрования
 */
class IEncryption {
public:
    virtual ~IEncryption() = default;
    
    /**
     * Инициализация алгоритма с ключом
     * @param key Ключ шифрования
     * @param key_size Размер ключа в байтах
     * @return true при успешной инициализации
     */
    virtual bool init(const unsigned char* key, size_t key_size) = 0;
    
    /**
     * Шифрование данных
     * @param data Данные для шифрования (изменяются на месте)
     * @param size Размер данных
     */
    virtual void encrypt(unsigned char* data, size_t size) = 0;
    
    /**
     * Дешифрование данных
     * @param data Данные для дешифрования (изменяются на месте)
     * @param size Размер данных
     */
    virtual void decrypt(unsigned char* data, size_t size) = 0;
    
    /**
     * Получение имени алгоритма
     * @return Название алгоритма
     */
    virtual const char* get_name() const = 0;
    
    /**
     * Получение версии алгоритма
     * @return Версия алгоритма
     */
    virtual const char* get_version() const = 0;
};

/**
 * Тип функции для создания экземпляра шифрования
 */
typedef IEncryption* (*create_encryption_func)();

/**
 * Тип функции для освобождения экземпляра шифрования
 */
typedef void (*destroy_encryption_func)(IEncryption* encryption);

// Стандартные имена функций в динамической библиотеке
extern "C" {
    IEncryption* create_encryption();
    void destroy_encryption(IEncryption* encryption);
}

#endif // ENCRYPTION_INTERFACE_H