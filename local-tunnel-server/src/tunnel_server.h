#ifndef TUNNEL_SERVER_H
#define TUNNEL_SERVER_H

#include "config.h"
#include "platform_compat.h"
#include "windows_threading.h"
#include <memory>
#include <atomic>
#include <vector>
#include <string>

// Форвард декларация
class ProxyHandler;

class TunnelServer {
public:
    struct TunnelStatus {
        bool running;
        int active_tunnels;
        std::string host;
        int port;
    };

    explicit TunnelServer(const Config& config);
    ~TunnelServer();

    bool start();
    void stop();
    bool is_running() const { return running_.load(); }
    TunnelStatus get_status() const;

private:
    void server_loop();
    void handle_tunnel_connection(SOCKET tunnel_socket, const std::string& client_ip, int client_port);
    void cleanup_finished_tunnels();

    static void signal_handler(int signal);
    static TunnelServer* instance_;

    const Config& config_;
    std::atomic<bool> running_{false};
    
    SOCKET server_socket_{INVALID_SOCKET};
    std::unique_ptr<thread_type> server_thread_;
    
    std::vector<std::shared_ptr<ProxyHandler>> tunnels_;
    mutable mutex_type tunnels_mutex_;
};

#endif // TUNNEL_SERVER_H