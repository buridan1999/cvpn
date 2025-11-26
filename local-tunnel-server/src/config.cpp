#include "config.h"
#include "logger.h"
#include <fstream>
#include <iostream>
#include <sstream>

Config::Config(const std::string& config_file) : config_file_(config_file) {
    set_defaults();
    load_config();
}

void Config::set_defaults() {
    // Серверные настройки по умолчанию
    server_host_ = "0.0.0.0";
    server_port_ = 8080;
    max_connections_ = 100;
    buffer_size_ = 4096;
    timeout_ = 30;
    
    // Туннельные настройки по умолчанию
    tunnel_host_ = "127.0.0.1";
    tunnel_port_ = 8081;
    xor_key_ = 42;
    
    // Настройки шифрования по умолчанию
    encryption_library_ = "./encryption_plugins/libxor_encryption.so";
    encryption_algorithm_ = "XOR";
    encryption_key_ = "DefaultKey123";
    
    // Настройки логирования по умолчанию
    log_level_ = "INFO";
    log_file_ = "vpn_server.log";
    log_format_ = "[%Y-%m-%d %H:%M:%S] [%l] %v";
    
    // Настройки аутентификации по умолчанию
    auth_enabled_ = false;
    username_ = "admin";
    password_ = "password123";
}

void Config::load_config() {
    try {
        if (parse_json_file()) {
            // Logger::info("Конфигурация успешно загружена из " + config_file_);
            std::cout << "Конфигурация успешно загружена из " + config_file_ << std::endl;
        } else {
            // Logger::warning("Не удалось загрузить конфигурацию из " + config_file_ + 
            //                ", используются настройки по умолчанию");
            std::cout << "Не удалось загрузить конфигурацию из " + config_file_ + 
                         ", используются настройки по умолчанию" << std::endl;
        }
    } catch (const std::exception& e) {
        // Logger::error("Ошибка при загрузке конфигурации: " + std::string(e.what()) + 
        //              ", используются настройки по умолчанию");
        std::cerr << "Ошибка при загрузке конфигурации: " + std::string(e.what()) + 
                     ", используются настройки по умолчанию" << std::endl;
    }
}

bool Config::parse_json_file() {
    std::ifstream file(config_file_);
    if (!file.is_open()) {
        return false;
    }

    std::string line, content;
    while (std::getline(file, line)) {
        content += line;
    }
    file.close();

    // Простой парсинг JSON (без библиотеки)
    // Ищем ключи и значения в формате "key": value
    
    // Парсинг серверных настроек
    if (content.find("\"host\"") != std::string::npos) {
        size_t start = content.find("\"host\"");
        size_t colon = content.find(":", start);
        size_t quote_start = content.find("\"", colon);
        size_t quote_end = content.find("\"", quote_start + 1);
        if (quote_start != std::string::npos && quote_end != std::string::npos) {
            server_host_ = content.substr(quote_start + 1, quote_end - quote_start - 1);
        }
    }
    
    if (content.find("\"port\"") != std::string::npos) {
        size_t start = content.find("\"port\"");
        size_t colon = content.find(":", start);
        size_t num_start = content.find_first_of("0123456789", colon);
        if (num_start != std::string::npos) {
            size_t num_end = content.find_first_not_of("0123456789", num_start);
            std::string port_str = content.substr(num_start, num_end - num_start);
            try {
                server_port_ = std::stoi(port_str);
            } catch (...) {}
        }
    }
    
    // Аналогично для других параметров...
    if (content.find("\"max_connections\"") != std::string::npos) {
        size_t start = content.find("\"max_connections\"");
        size_t colon = content.find(":", start);
        size_t num_start = content.find_first_of("0123456789", colon);
        if (num_start != std::string::npos) {
            size_t num_end = content.find_first_not_of("0123456789", num_start);
            std::string val_str = content.substr(num_start, num_end - num_start);
            try {
                max_connections_ = std::stoi(val_str);
            } catch (...) {}
        }
    }
    
    if (content.find("\"buffer_size\"") != std::string::npos) {
        size_t start = content.find("\"buffer_size\"");
        size_t colon = content.find(":", start);
        size_t num_start = content.find_first_of("0123456789", colon);
        if (num_start != std::string::npos) {
            size_t num_end = content.find_first_not_of("0123456789", num_start);
            std::string val_str = content.substr(num_start, num_end - num_start);
            try {
                buffer_size_ = std::stoi(val_str);
            } catch (...) {}
        }
    }
    
    if (content.find("\"timeout\"") != std::string::npos) {
        size_t start = content.find("\"timeout\"");
        size_t colon = content.find(":", start);
        size_t num_start = content.find_first_of("0123456789", colon);
        if (num_start != std::string::npos) {
            size_t num_end = content.find_first_not_of("0123456789", num_start);
            std::string val_str = content.substr(num_start, num_end - num_start);
            try {
                timeout_ = std::stoi(val_str);
            } catch (...) {}
        }
    }
    
    // Парсинг туннельных настроек
    size_t tunnel_section = content.find("\"tunnel\"");
    if (tunnel_section != std::string::npos) {
        size_t tunnel_start = content.find("{", tunnel_section);
        size_t tunnel_end = content.find("}", tunnel_start);
        if (tunnel_start != std::string::npos && tunnel_end != std::string::npos) {
            std::string tunnel_content = content.substr(tunnel_start, tunnel_end - tunnel_start);
            
            // Парсинг tunnel host
            if (tunnel_content.find("\"host\"") != std::string::npos) {
                size_t start = tunnel_content.find("\"host\"");
                size_t colon = tunnel_content.find(":", start);
                size_t quote_start = tunnel_content.find("\"", colon);
                size_t quote_end = tunnel_content.find("\"", quote_start + 1);
                if (quote_start != std::string::npos && quote_end != std::string::npos) {
                    tunnel_host_ = tunnel_content.substr(quote_start + 1, quote_end - quote_start - 1);
                }
            }
            
            // Парсинг tunnel port
            if (tunnel_content.find("\"port\"") != std::string::npos) {
                size_t start = tunnel_content.find("\"port\"");
                size_t colon = tunnel_content.find(":", start);
                size_t num_start = tunnel_content.find_first_of("0123456789", colon);
                if (num_start != std::string::npos) {
                    size_t num_end = tunnel_content.find_first_not_of("0123456789", num_start);
                    std::string port_str = tunnel_content.substr(num_start, num_end - num_start);
                    try {
                        tunnel_port_ = std::stoi(port_str);
                    } catch (...) {}
                }
            }
            
            // Парсинг XOR key
            if (tunnel_content.find("\"xor_key\"") != std::string::npos) {
                size_t start = tunnel_content.find("\"xor_key\"");
                size_t colon = tunnel_content.find(":", start);
                size_t num_start = tunnel_content.find_first_of("0123456789", colon);
                if (num_start != std::string::npos) {
                    size_t num_end = tunnel_content.find_first_not_of("0123456789", num_start);
                    std::string key_str = tunnel_content.substr(num_start, num_end - num_start);
                    try {
                        xor_key_ = static_cast<unsigned char>(std::stoi(key_str));
                    } catch (...) {}
                }
            }
        }
    }
    
    // Парсинг настроек шифрования
    size_t encryption_section = content.find("\"encryption\"");
    if (encryption_section != std::string::npos) {
        size_t encryption_start = content.find("{", encryption_section);
        size_t encryption_end = content.find("}", encryption_start);
        if (encryption_start != std::string::npos && encryption_end != std::string::npos) {
            std::string encryption_content = content.substr(encryption_start, encryption_end - encryption_start);
            
            // Парсинг library_path
            if (encryption_content.find("\"library_path\"") != std::string::npos) {
                size_t start = encryption_content.find("\"library_path\"");
                size_t colon = encryption_content.find(":", start);
                size_t quote_start = encryption_content.find("\"", colon);
                size_t quote_end = encryption_content.find("\"", quote_start + 1);
                if (quote_start != std::string::npos && quote_end != std::string::npos) {
                    encryption_library_ = encryption_content.substr(quote_start + 1, quote_end - quote_start - 1);
                }
            }
            
            // Парсинг algorithm
            if (encryption_content.find("\"algorithm\"") != std::string::npos) {
                size_t start = encryption_content.find("\"algorithm\"");
                size_t colon = encryption_content.find(":", start);
                size_t quote_start = encryption_content.find("\"", colon);
                size_t quote_end = encryption_content.find("\"", quote_start + 1);
                if (quote_start != std::string::npos && quote_end != std::string::npos) {
                    encryption_algorithm_ = encryption_content.substr(quote_start + 1, quote_end - quote_start - 1);
                }
            }
            
            // Парсинг key
            if (encryption_content.find("\"key\"") != std::string::npos) {
                size_t start = encryption_content.find("\"key\"");
                size_t colon = encryption_content.find(":", start);
                size_t quote_start = encryption_content.find("\"", colon);
                size_t quote_end = encryption_content.find("\"", quote_start + 1);
                if (quote_start != std::string::npos && quote_end != std::string::npos) {
                    encryption_key_ = encryption_content.substr(quote_start + 1, quote_end - quote_start - 1);
                }
            }
        }
    }
    
    return true;
}

bool Config::reload() {
    try {
        load_config();
        return true;
    } catch (const std::exception& e) {
        // Logger::error("Ошибка при перезагрузке конфигурации: " + std::string(e.what()));
        std::cerr << "Ошибка при перезагрузке конфигурации: " + std::string(e.what()) << std::endl;
        return false;
    }
}