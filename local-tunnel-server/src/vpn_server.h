#ifndef VPN_SERVER_H
#define VPN_SERVER_H

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include "config.h"
#include "proxy_handler.h"
#include <thread>
#include <mutex>

class VPNServer {
public:
    explicit VPNServer(const std::string& config_file = "config.json");
    ~VPNServer();

    // Основные методы
    bool start();
    void stop();
    bool is_running() const { return running_.load(); }

    // Статус сервера
    struct ServerStatus {
        bool running;
        int active_clients;
        std::string host;
        int port;
    };
    
    ServerStatus get_status() const;

private:
    // Конфигурация и состояние
    Config config_;
    std::atomic<bool> running_{false};
    int server_socket_{-1};
    
    // Управление клиентами
    std::vector<std::shared_ptr<ProxyHandler>> clients_;
    mutable std::mutex clients_mutex_;
    
    // Основной поток сервера
    std::unique_ptr<std::thread> server_thread_;
    
    // Внутренние методы
    void server_loop();
    void handle_client_connection(int client_socket, const std::string& client_ip, int client_port);
    void remove_client(std::shared_ptr<ProxyHandler> handler);
    void cleanup_finished_clients();
    
    // Обработка сигналов
    static void signal_handler(int signal);
    static VPNServer* instance_;
    
    // Запрет копирования
    VPNServer(const VPNServer&) = delete;
    VPNServer& operator=(const VPNServer&) = delete;
};

#endif // VPN_SERVER_H