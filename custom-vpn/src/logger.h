#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARNING = 3,
    ERROR = 4,
    CRITICAL = 5,
    OFF = 6
};

class Logger {
public:
    static void init(const std::string& level = "INFO", 
                    const std::string& file = "", 
                    const std::string& format = "");
    
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);
    static void debug(const std::string& message);
    
    static void set_level(const std::string& level);

private:
    static LogLevel current_level_;
    static std::string log_file_;
    static std::ofstream file_stream_;
    static std::mutex log_mutex_;
    
    static LogLevel string_to_level(const std::string& level);
    static std::string level_to_string(LogLevel level);
    static void log(LogLevel level, const std::string& message);
    static std::string get_timestamp();
};

#endif // LOGGER_H