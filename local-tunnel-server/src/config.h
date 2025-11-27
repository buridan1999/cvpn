#ifndef CONFIG_H
#define CONFIG_H

#include <string>

class Config {
public:
    enum class ServerMode {
        BOTH,       // Запуск и VPNServer (HTTP прокси на 8080), и TunnelServer (удалённая часть на 8081) 
        PROXY_ONLY, // Запуск только VPNServer (HTTP прокси на 8080 для браузера)
        TUNNEL_ONLY // Запуск только TunnelServer (удалённая часть на 8081)
    };
    
    explicit Config(const std::string& config_file = "config.json");
    
    // Геттеры для серверных настроек
    ServerMode get_server_mode() const { return server_mode_; }
    std::string get_server_host() const { return server_host_; }
    int get_server_port() const { return server_port_; }
    int get_max_connections() const { return max_connections_; }
    int get_buffer_size() const { return buffer_size_; }
    int get_timeout() const { return timeout_; }
    
    // Геттеры для туннельных настроек
    std::string get_tunnel_host() const { return tunnel_host_; }
    int get_tunnel_port() const { return tunnel_port_; }
    unsigned char get_xor_key() const { return xor_key_; }
    
    // Геттеры для настроек шифрования
    std::string get_encryption_library() const { return encryption_library_; }
    std::string get_encryption_algorithm() const { return encryption_algorithm_; }
    std::string get_encryption_key() const { return encryption_key_; }
    
    // Геттеры для настроек логирования
    std::string get_log_level() const { return log_level_; }
    std::string get_log_file() const { return log_file_; }
    std::string get_log_format() const { return log_format_; }
    
    // Геттеры для аутентификации
    bool is_auth_enabled() const { return auth_enabled_; }
    std::string get_username() const { return username_; }
    std::string get_password() const { return password_; }
    
    // Перезагрузка конфигурации
    bool reload();
    
private:
    std::string config_file_;
    
    // Режим работы сервера
    ServerMode server_mode_;
    
    // Серверные настройки
    std::string server_host_;
    int server_port_;
    int max_connections_;
    int buffer_size_;
    int timeout_;
    
    // Туннельные настройки
    std::string tunnel_host_;
    int tunnel_port_;
    unsigned char xor_key_;
    
    // Настройки шифрования
    std::string encryption_library_;
    std::string encryption_algorithm_;
    std::string encryption_key_;
    
    // Настройки логирования
    std::string log_level_;
    std::string log_file_;
    std::string log_format_;
    
    // Настройки аутентификации
    bool auth_enabled_;
    std::string username_;
    std::string password_;
    
    // Загрузка конфигурации
    void load_config();
    void set_defaults();
    bool parse_json_file();
};

#endif // CONFIG_H