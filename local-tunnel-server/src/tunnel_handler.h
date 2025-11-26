#ifndef TUNNEL_HANDLER_H
#define TUNNEL_HANDLER_H

#include "config.h"
#include "encryption_manager.h"
#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <vector>

class TunnelHandler {
public:
    TunnelHandler(int tunnel_socket, const std::string& client_ip,
                 int client_port, const Config& config);
    ~TunnelHandler() noexcept;

    bool start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    void handle();
    bool read_mutated_data(std::string& target_host, int& target_port, std::vector<char>& initial_data);
    bool connect_to_target(const std::string& host, int port);
    void start_data_transfer(const std::vector<char>& initial_data);
    void transfer_data_to_target(int source_socket, int destination_socket);
    void transfer_data_from_target(int source_socket, int destination_socket);
    
    // XOR функции для мутации/демутации данных
    void decrypt(unsigned char* data, size_t size);
    void encrypt(unsigned char* data, size_t size);
    
    // Менеджер шифрования
    EncryptionManager encryption_manager_;
    
    const Config& config_;
    
    int tunnel_socket_;
    std::string client_ip_;
    int client_port_;
    
    int target_socket_{-1};
    std::atomic<bool> running_{false};
    
    std::unique_ptr<std::thread> handler_thread_;
    std::unique_ptr<std::thread> to_target_thread_;
    std::unique_ptr<std::thread> from_target_thread_;
};

#endif // TUNNEL_HANDLER_H