#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>

LogLevel Logger::current_level_ = LogLevel::INFO;
std::string Logger::log_file_;
std::ofstream Logger::file_stream_;
std::mutex Logger::log_mutex_;

void Logger::init(const std::string& level, const std::string& file, 
                 const std::string& format) {
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    // Установка уровня логирования
    current_level_ = string_to_level(level);
    
    // Настройка файлового вывода
    if (!file.empty()) {
        log_file_ = file;
        if (file_stream_.is_open()) {
            file_stream_.close();
        }
        file_stream_.open(log_file_, std::ios::app);
    }
    
    // Тестовое сообщение инициализации
    std::cout << get_timestamp() << " [INFO] Система логирования инициализирована" << std::endl;
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::set_level(const std::string& level) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    current_level_ = string_to_level(level);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < current_level_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    std::string timestamp = get_timestamp();
    std::string level_str = level_to_string(level);
    std::string log_line = timestamp + " [" + level_str + "] " + message;
    
    // Вывод в консоль
    if (level >= LogLevel::ERROR) {
        std::cerr << log_line << std::endl;
    } else {
        std::cout << log_line << std::endl;
    }
    
    // Вывод в файл
    if (file_stream_.is_open()) {
        file_stream_ << log_line << std::endl;
        file_stream_.flush();
    }
}

LogLevel Logger::string_to_level(const std::string& level) {
    if (level == "TRACE") return LogLevel::TRACE;
    if (level == "DEBUG") return LogLevel::DEBUG;
    if (level == "INFO") return LogLevel::INFO;
    if (level == "WARN" || level == "WARNING") return LogLevel::WARNING;
    if (level == "ERROR") return LogLevel::ERROR;
    if (level == "CRITICAL") return LogLevel::CRITICAL;
    if (level == "OFF") return LogLevel::OFF;
    
    return LogLevel::INFO; // По умолчанию
}

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRIT";
        case LogLevel::OFF: return "OFF";
    }
    return "INFO";
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}