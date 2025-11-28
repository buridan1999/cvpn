#ifndef ENCRYPTION_MANAGER_H
#define ENCRYPTION_MANAGER_H

#include "encryption_interface.h"
#include <memory>
#include <string>

/**
 * Менеджер для загрузки и управления алгоритмами шифрования
 */
class EncryptionManager {
public:
    EncryptionManager();
    ~EncryptionManager();
    
    /**
     * Загрузка алгоритма шифрования из динамической библиотеки
     * @param library_path Путь к библиотеке (например, "./libxor_encryption")
     * @param key Ключ для инициализации
     * @param key_size Размер ключа
     * @return true при успешной загрузке
     */
    bool load_encryption(const std::string& library_path, 
                        const unsigned char* key, 
                        size_t key_size);
    
    /**
     * Выгрузка текущего алгоритма
     */
    void unload_encryption();
    
    /**
     * Проверка, загружен ли алгоритм
     */
    bool is_loaded() const { return encryption_ != nullptr; }
    
    /**
     * Шифрование данных
     */
    void encrypt(unsigned char* data, size_t size);
    
    /**
     * Дешифрование данных
     */
    void decrypt(unsigned char* data, size_t size);
    
    /**
     * Получение информации об алгоритме
     */
    std::string get_algorithm_info() const;
    
private:
    void* library_handle_;  // Дескриптор динамической библиотеки
    IEncryption* encryption_; // Экземпляр алгоритма шифрования
    destroy_encryption_func destroy_func_; // Функция для освобождения
    
    // Запрет копирования
    EncryptionManager(const EncryptionManager&) = delete;
    EncryptionManager& operator=(const EncryptionManager&) = delete;
};

#endif // ENCRYPTION_MANAGER_H