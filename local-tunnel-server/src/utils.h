#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

namespace Utils {
    // Проверка IP адреса
    bool is_valid_ip(const std::string& ip);
    
    // Проверка порта
    bool is_valid_port(int port);
    
    // Разбор строки адреса вида "host:port"
    bool parse_address(const std::string& address, std::string& host, int& port);
    
    // Получение текущего времени в строковом формате
    std::string get_current_time();
    
    // Преобразование байт в читаемый формат
    std::string format_bytes(size_t bytes);
    
    // Разбиение строки по разделителю
    std::vector<std::string> split(const std::string& str, char delimiter);
    
    // Удаление пробелов с начала и конца строки
    std::string trim(const std::string& str);
    
    // Проверка существования файла
    bool file_exists(const std::string& filename);
}

#endif // UTILS_H