#ifndef PROXY_HANDLER_H
#define PROXY_HANDLER_H

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include "config.h"

class ProxyHandler {
public:
    ProxyHandler(int client_socket, const std::string& client_ip, 
                int client_port, const Config& config);
    ~ProxyHandler() noexcept;

    // Основные методы
    bool start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Информация о клиенте
    std::string get_client_ip() const { return client_ip_; }
    int get_client_port() const { return client_port_; }

private:
    // Состояние соединения
    std::atomic<bool> running_{false};
    bool is_http_connect_{false};
    std::string original_http_request_;
    
    // Сокеты
    int client_socket_{-1};
    int target_socket_{-1};
    
    // Информация о клиенте
    std::string client_ip_;
    int client_port_;
    
    // Конфигурация
    const Config& config_;
    
    // Потоки для передачи данных
    std::unique_ptr<std::thread> handler_thread_;
    
    // Внутренние методы
    void handle();
    bool get_target_info(std::string& target_host, int& target_port);
    bool parse_http_connect(const std::string& connect_line, std::string& target_host, int& target_port);
    bool parse_http_request(const std::string& request_line, std::string& target_host, int& target_port);
    bool parse_binary_protocol_from_buffer(char* buffer, int buffer_size, std::string& target_host, int& target_port);
    bool connect_to_target(const std::string& host, int port);
    void send_connection_response(bool success);
    void send_http_response(bool success);
    void forward_http_request();
    void start_data_transfer();
    void transfer_data(int source_socket, int destination_socket, 
                      const std::string& direction);
    ssize_t recv_exact(int socket, void* buffer, size_t size);
    
    // Запрет копирования
    ProxyHandler(const ProxyHandler&) = delete;
    ProxyHandler& operator=(const ProxyHandler&) = delete;
};

#endif // PROXY_HANDLER_H