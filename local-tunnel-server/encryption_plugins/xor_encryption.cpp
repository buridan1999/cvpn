#include "../src/encryption_interface.h"
#include "encryption_export.h"
#include <cstring>

/**
 * Реализация XOR шифрования
 */
class XOREncryption : public IEncryption {
private:
    unsigned char key_;
    bool initialized_;
    
public:
    XOREncryption() : key_(0), initialized_(false) {}
    
    bool init(const unsigned char* key, size_t key_size) override {
        if (key_size == 0) {
            return false;
        }
        
        // Для XOR используем первый байт ключа
        key_ = key[0];
        initialized_ = true;
        return true;
    }
    
    void encrypt(unsigned char* data, size_t size) override {
        if (!initialized_) return;
        
        for (size_t i = 0; i < size; ++i) {
            data[i] ^= key_;
        }
    }
    
    void decrypt(unsigned char* data, size_t size) override {
        // Для XOR дешифрование = шифрование
        encrypt(data, size);
    }
    
    const char* get_name() const override {
        return "XOR";
    }
    
    const char* get_version() const override {
        return "1.0.0";
    }
};

// Экспортируемые функции
extern "C" {
    ENCRYPTION_API IEncryption* ENCRYPTION_CALL create_encryption() {
        return new XOREncryption();
    }

    ENCRYPTION_API void ENCRYPTION_CALL destroy_encryption(IEncryption* encryption) {
        delete encryption;
    }
}