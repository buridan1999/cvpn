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