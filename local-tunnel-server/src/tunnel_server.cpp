#include "tunnel_server.h"
#include "tunnel_handler.h"
#include "logger.h"
#include "platform_compat.h"
#include <cstring>
#include <cerrno>
#include <csignal>
#include <chrono>
#include <iomanip>
#include <sstream>

TunnelServer* TunnelServer::instance_ = nullptr;

TunnelServer::TunnelServer(const Config& config) : config_(config) {
    instance_ = this;
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    Logger::info("Tunnel сервер инициализирован");
}

TunnelServer::~TunnelServer() {
    stop();
    instance_ = nullptr;
}

bool TunnelServer::start() {
    if (running_.load()) {
        Logger::warning("Tunnel сервер уже запущен");
        return false;
    }

    // Создание серверного сокета
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        Logger::error("Не удалось создать tunnel серверный сокет");
        return false;
    }

    // Настройка сокета для повторного использования адреса
    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        Logger::error("Не удалось настроить SO_REUSEADDR для tunnel сервера");
        close(server_socket_);
        return false;
    }

    // Настройка адреса сервера
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config_.get_tunnel_port());
    
    if (config_.get_tunnel_host() == "0.0.0.0") {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, config_.get_tunnel_host().c_str(), &server_addr.sin_addr) <= 0) {
            Logger::error("Некорректный IP адрес tunnel сервера: " + config_.get_tunnel_host());
            close(server_socket_);
            return false;
        }
    }

    // Привязка сокета к адресу
    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        Logger::error("Не удалось привязать tunnel сокет к адресу " + 
                     config_.get_tunnel_host() + ":" + std::to_string(config_.get_tunnel_port()));
        close(server_socket_);
        return false;
    }

    // Начало прослушивания
    if (listen(server_socket_, config_.get_max_connections()) < 0) {
        Logger::error("Не удалось начать прослушивание tunnel сокета");
        close(server_socket_);
        return false;
    }

    running_.store(true);
    
    // Запуск серверного потока
    server_thread_ = std::make_unique<std::thread>(&TunnelServer::server_loop, this);

    Logger::info("Tunnel сервер запущен на " + config_.get_tunnel_host() + 
                ":" + std::to_string(config_.get_tunnel_port()));

    return true;
}

void TunnelServer::stop() {
    if (!running_.load()) {
        return;
    }

    Logger::info("Остановка Tunnel сервера...");
    running_.store(false);

    // Закрытие серверного сокета
    if (server_socket_ >= 0) {
        close(server_socket_);
        server_socket_ = -1;
    }

    // Ожидание завершения серверного потока
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }

    // Закрытие всех туннельных соединений
    {
        std::lock_guard<std::mutex> lock(tunnels_mutex_);
        for (auto& tunnel : tunnels_) {
            tunnel->stop();
        }
        tunnels_.clear();
    }

    Logger::info("Tunnel сервер остановлен");
}

void TunnelServer::server_loop() {
    while (running_.load()) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket_, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ready = select(server_socket_ + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (ready < 0) {
            if (errno == EINTR) {
                break;
            }
            if (running_.load()) {
                Logger::error("Ошибка select в tunnel сервере: " + std::string(strerror(errno)));
            }
            continue;
        }
        
        if (ready == 0) {
            cleanup_finished_tunnels();
            continue;
        }
        
        if (!FD_ISSET(server_socket_, &read_fds)) {
            continue;
        }

        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int tunnel_socket = accept(server_socket_, 
                                  reinterpret_cast<sockaddr*>(&client_addr), 
                                  &client_len);

        if (tunnel_socket < 0) {
            if (errno == EINTR || errno == EWOULDBLOCK) {
                continue;
            }
            if (running_.load()) {
                Logger::error("Ошибка при принятии tunnel соединения: " + std::string(strerror(errno)));
            }
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        Logger::info("Новое tunnel соединение от " + std::string(client_ip) + 
                    ":" + std::to_string(client_port));

        handle_tunnel_connection(tunnel_socket, client_ip, client_port);
        cleanup_finished_tunnels();
    }
}

void TunnelServer::handle_tunnel_connection(int tunnel_socket, 
                                          const std::string& client_ip, 
                                          int client_port) {
    try {
        std::lock_guard<std::mutex> lock(tunnels_mutex_);
        if (static_cast<int>(tunnels_.size()) >= config_.get_max_connections()) {
            Logger::warning("Достигнут лимит tunnel соединений, отклонение " + 
                           client_ip + ":" + std::to_string(client_port));
            close(tunnel_socket);
            return;
        }

        auto handler = std::make_shared<TunnelHandler>(tunnel_socket, client_ip, client_port, config_);
        
        if (handler->start()) {
            tunnels_.push_back(handler);
        } else {
            Logger::error("Не удалось запустить tunnel обработчик для " + 
                         client_ip + ":" + std::to_string(client_port));
            close(tunnel_socket);
        }
    } catch (const std::exception& e) {
        Logger::error("Ошибка обработки tunnel соединения: " + std::string(e.what()));
        close(tunnel_socket);
    }
}

void TunnelServer::cleanup_finished_tunnels() {
    std::lock_guard<std::mutex> lock(tunnels_mutex_);
    
    auto it = tunnels_.begin();
    while (it != tunnels_.end()) {
        if (!(*it)->is_running()) {
            try {
                (*it)->stop();
                it = tunnels_.erase(it);
            } catch (...) {
                it = tunnels_.erase(it);
            }
        } else {
            ++it;
        }
    }
}

TunnelServer::TunnelStatus TunnelServer::get_status() const {
    std::lock_guard<std::mutex> lock(tunnels_mutex_);
    
    return TunnelStatus{
        running_.load(),
        static_cast<int>(tunnels_.size()),
        config_.get_tunnel_host(),
        config_.get_tunnel_port()
    };
}

void TunnelServer::signal_handler(int signal) {
    if (instance_) {
        Logger::info("Tunnel сервер получил сигнал " + std::to_string(signal));
        instance_->stop();
    }
}