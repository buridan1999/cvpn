#include "../src/encryption_interface.h"
#include <cstring>

/**
 * Реализация Caesar cipher шифрования
 */
class CaesarEncryption : public IEncryption {
private:
    unsigned char shift_;
    bool initialized_;
    
public:
    CaesarEncryption() : shift_(0), initialized_(false) {}
    
    bool init(const unsigned char* key, size_t key_size) override {
        if (key_size == 0) {
            return false;
        }
        
        // Используем первый байт ключа как сдвиг
        shift_ = key[0] % 256;
        initialized_ = true;
        return true;
    }
    
    void encrypt(unsigned char* data, size_t size) override {
        if (!initialized_) return;
        
        for (size_t i = 0; i < size; ++i) {
            data[i] = (data[i] + shift_) % 256;
        }
    }
    
    void decrypt(unsigned char* data, size_t size) override {
        if (!initialized_) return;
        
        for (size_t i = 0; i < size; ++i) {
            data[i] = (data[i] - shift_ + 256) % 256;
        }
    }
    
    const char* get_name() const override {
        return "Caesar";
    }
    
    const char* get_version() const override {
        return "1.0.0";
    }
};

// Экспортируемые функции
extern "C" {
    IEncryption* create_encryption() {
        return new CaesarEncryption();
    }
    
    void destroy_encryption(IEncryption* encryption) {
        delete encryption;
    }
}