#include "utils.h"
#include <regex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>

namespace Utils {

bool is_valid_ip(const std::string& ip) {
    // Простая проверка IPv4 адреса с помощью регулярного выражения
    std::regex ip_regex(R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    return std::regex_match(ip, ip_regex);
}

bool is_valid_port(int port) {
    return port >= 1 && port <= 65535;
}

bool parse_address(const std::string& address, std::string& host, int& port) {
    size_t colon_pos = address.find_last_of(':');
    if (colon_pos == std::string::npos) {
        return false;
    }
    
    host = address.substr(0, colon_pos);
    std::string port_str = address.substr(colon_pos + 1);
    
    try {
        port = std::stoi(port_str);
        return is_valid_port(port);
    } catch (const std::exception&) {
        return false;
    }
}

std::string get_current_time() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string format_bytes(size_t bytes) {
    const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    int suffix_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && suffix_index < 4) {
        size /= 1024.0;
        suffix_index++;
    }
    
    std::stringstream ss;
    if (suffix_index == 0) {
        ss << static_cast<size_t>(size) << " " << suffixes[suffix_index];
    } else {
        ss << std::fixed << std::setprecision(2) << size << " " << suffixes[suffix_index];
    }
    
    return ss.str();
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string trim(const std::string& str) {
    auto start = std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });
    
    auto end = std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base();
    
    return (start < end) ? std::string(start, end) : std::string();
}

bool file_exists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

} // namespace Utils