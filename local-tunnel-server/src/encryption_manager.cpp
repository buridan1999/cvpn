#include "encryption_manager.h"
#include "logger.h"
#include <cstring>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

EncryptionManager::EncryptionManager() 
    : library_handle_(nullptr), encryption_(nullptr), destroy_func_(nullptr) {
}

EncryptionManager::~EncryptionManager() {
    unload_encryption();
}

bool EncryptionManager::load_encryption(const std::string& library_path, 
                                       const unsigned char* key, 
                                       size_t key_size) {
    // Выгружаем предыдущий алгоритм, если был загружен
    unload_encryption();

    Logger::info("Попытка загрузки библиотеки шифрования: " + library_path);
    
    // Добавляем правильное расширение в зависимости от платформы, если его нет
    std::string full_library_path = library_path;
#ifdef _WIN32
    if (full_library_path.find(".dll") == std::string::npos) {
        full_library_path += ".dll";
    }
#else
    if (full_library_path.find(".so") == std::string::npos) {
        full_library_path += ".so";
    }
#endif
    
#ifdef _WIN32
    // Загружаем динамическую библиотеку (Windows)
    library_handle_ = LoadLibraryA(full_library_path.c_str());
    if (!library_handle_) {
        DWORD error = GetLastError();
        Logger::error("Не удалось загрузить библиотеку шифрования: " + full_library_path + 
                     " - Ошибка: " + std::to_string(error));
        return false;
    }
    
    // Получаем функцию создания
    create_encryption_func create_func = 
        (create_encryption_func) GetProcAddress((HMODULE)library_handle_, "create_encryption");
    
    if (!create_func) {
        Logger::error("Не найдена функция create_encryption в библиотеке");
        FreeLibrary((HMODULE)library_handle_);
        library_handle_ = nullptr;
        return false;
    }
    
    // Получаем функцию удаления
    destroy_func_ = (destroy_encryption_func) GetProcAddress((HMODULE)library_handle_, "destroy_encryption");
    if (!destroy_func_) {
        Logger::error("Не найдена функция destroy_encryption в библиотеке");
        FreeLibrary((HMODULE)library_handle_);
        library_handle_ = nullptr;
        return false;
    }
#else
    // Загружаем динамическую библиотеку (Unix/Linux)
    library_handle_ = dlopen(full_library_path.c_str(), RTLD_LAZY);
    if (!library_handle_) {
        Logger::error("Не удалось загрузить библиотеку шифрования: " + full_library_path + 
                     " - " + std::string(dlerror()));
        return false;
    }
    
    // Очищаем ошибки dlsym
    dlerror();
    
    // Получаем функцию создания
    create_encryption_func create_func = 
        (create_encryption_func) dlsym(library_handle_, "create_encryption");
    
    char* error = dlerror();
    if (error != nullptr) {
        Logger::error("Не найдена функция create_encryption: " + std::string(error));
        dlclose(library_handle_);
        library_handle_ = nullptr;
        return false;
    }
    
    // Получаем функцию удаления
    destroy_func_ = (destroy_encryption_func) dlsym(library_handle_, "destroy_encryption");
    error = dlerror();
    if (error != nullptr) {
        Logger::error("Не найдена функция destroy_encryption: " + std::string(error));
        dlclose(library_handle_);
        library_handle_ = nullptr;
        return false;
    }
#endif
    
    // Создаем экземпляр алгоритма
    encryption_ = create_func();
    if (!encryption_) {
        Logger::error("Не удалось создать экземпляр алгоритма шифрования");
#ifdef _WIN32
        FreeLibrary((HMODULE)library_handle_);
#else
        dlclose(library_handle_);
#endif
        library_handle_ = nullptr;
        destroy_func_ = nullptr;
        return false;
    }
    
    // Инициализируем алгоритм
    if (!encryption_->init(key, key_size)) {
        Logger::error("Не удалось инициализировать алгоритм шифрования");
        destroy_func_(encryption_);
        encryption_ = nullptr;
#ifdef _WIN32
        FreeLibrary((HMODULE)library_handle_);
#else
        dlclose(library_handle_);
#endif
        library_handle_ = nullptr;
        destroy_func_ = nullptr;
        return false;
    }
    
    Logger::info("Загружен алгоритм шифрования: " + std::string(encryption_->get_name()) + 
                " v" + std::string(encryption_->get_version()) + 
                " из " + full_library_path);
    
    return true;
}

void EncryptionManager::unload_encryption() {
    if (encryption_ && destroy_func_) {
        destroy_func_(encryption_);
        encryption_ = nullptr;
        destroy_func_ = nullptr;
    }
    
    if (library_handle_) {
#ifdef _WIN32
        FreeLibrary((HMODULE)library_handle_);
#else
        dlclose(library_handle_);
#endif
        library_handle_ = nullptr;
    }
}

void EncryptionManager::encrypt(unsigned char* data, size_t size) {
    if (encryption_) {
        encryption_->encrypt(data, size);
    }
}

void EncryptionManager::decrypt(unsigned char* data, size_t size) {
    if (encryption_) {
        encryption_->decrypt(data, size);
    }
}

std::string EncryptionManager::get_algorithm_info() const {
    if (encryption_) {
        return std::string(encryption_->get_name()) + " v" + 
               std::string(encryption_->get_version());
    }
    return "Не загружен";
}