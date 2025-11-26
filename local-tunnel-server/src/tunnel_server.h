#ifndef TUNNEL_SERVER_H
#define TUNNEL_SERVER_H

#include "config.h"
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <string>

// Форвард декларация
class TunnelHandler;

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
    void handle_tunnel_connection(int tunnel_socket, const std::string& client_ip, int client_port);
    void cleanup_finished_tunnels();

    static void signal_handler(int signal);
    static TunnelServer* instance_;

    const Config& config_;
    std::atomic<bool> running_{false};
    
    int server_socket_{-1};
    std::unique_ptr<std::thread> server_thread_;
    
    std::vector<std::shared_ptr<TunnelHandler>> tunnels_;
    mutable std::mutex tunnels_mutex_;
};

#endif // TUNNEL_SERVER_H