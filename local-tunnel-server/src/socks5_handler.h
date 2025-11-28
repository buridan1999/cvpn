#pragma once

#include <string>
#include <atomic>
#include <memory>
#include "platform_compat.h"
#include "config.h"
#include "encryption_manager.h"
#include "windows_threading.h"

class Socks5Handler {
public:
    Socks5Handler(int client_socket, const std::string& client_ip, 
                  int client_port, const Config& config);
    ~Socks5Handler() noexcept;

    bool start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    void handle();
    bool handle_handshake();
    bool handle_connect_request(std::string& target_host, int& target_port);
    bool connect_to_tunnel(const std::string& target_host, int target_port);
    void send_socks5_response(uint8_t reply_code, const std::string& bind_addr = "0.0.0.0", uint16_t bind_port = 0);
    void data_transfer();

    SOCKET client_socket_;
    SOCKET tunnel_socket_;
    std::string client_ip_;
    int client_port_;
    const Config& config_;
    std::atomic<bool> running_{false};
    std::unique_ptr<thread_type> handler_thread_;
    EncryptionManager encryption_manager_;

    // SOCKS5 command constants
    static constexpr uint8_t SOCKS5_VERSION = 0x05;
    static constexpr uint8_t SOCKS5_NO_AUTH = 0x00;
    static constexpr uint8_t SOCKS5_CMD_CONNECT = 0x01;
    static constexpr uint8_t SOCKS5_ATYP_IPV4 = 0x01;
    static constexpr uint8_t SOCKS5_ATYP_DOMAIN = 0x03;
    static constexpr uint8_t SOCKS5_REP_SUCCESS = 0x00;
    static constexpr uint8_t SOCKS5_REP_FAILURE = 0x01;
};