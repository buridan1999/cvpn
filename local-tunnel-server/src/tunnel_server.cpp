#include "tunnel_server.h"
#include "proxy_handler.h"
#include "socks5_handler.h"
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
    if (server_socket_ == INVALID_SOCKET) {
        int error = get_last_socket_error();
        Logger::error("Не удалось создать tunnel серверный сокет: " + std::to_string(error));
        return false;
    }

    // Настройка сокета для повторного использования адреса
    if (set_socket_reuse(server_socket_) != 0) {
        int error = get_last_socket_error();
        Logger::error("Не удалось настроить SO_REUSEADDR для tunnel сервера: " + std::to_string(error));
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
        if (inet_pton_compat(AF_INET, config_.get_tunnel_host().c_str(), &server_addr.sin_addr) <= 0) {
            Logger::error("Некорректный IP адрес tunnel сервера: " + config_.get_tunnel_host());
            close(server_socket_);
            return false;
        }
    }

    // Привязка сокета к адресу
    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) != 0) {
        int error = get_last_socket_error();
        Logger::error("Не удалось привязать tunnel сокет к адресу " + 
                     config_.get_tunnel_host() + ":" + std::to_string(config_.get_tunnel_port()) +
                     ", ошибка: " + std::to_string(error));
        close(server_socket_);
        return false;
    }

    // Начало прослушивания
    if (listen(server_socket_, config_.get_max_connections()) != 0) {
        int error = get_last_socket_error();
        Logger::error("Не удалось начать прослушивание tunnel сокета: " + std::to_string(error));
        close(server_socket_);
        return false;
    }

    running_.store(true);
    
    // Запуск серверного потока
    server_thread_ = threading::make_unique_thread(&TunnelServer::server_loop, this);

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
    if (server_socket_ != INVALID_SOCKET) {
        close(server_socket_);
        server_socket_ = INVALID_SOCKET;
    }

    // Безопасное ожидание завершения серверного потока
    try {
        if (server_thread_ && server_thread_->joinable()) {
            server_thread_->join();
        }
    } catch (...) {
        // Игнорируем ошибки при join
    }

    // Закрытие всех туннельных соединений
    try {
        lock_guard_type<mutex_type> lock(tunnels_mutex_);
        
        // Остановка HTTP туннелей
        for (auto& tunnel : http_tunnels_) {
            try {
                if (tunnel) {
                    tunnel->stop();
                }
            } catch (...) {
                // Игнорируем ошибки остановки туннелей
            }
        }
        http_tunnels_.clear();
        
        // Остановка SOCKS5 туннелей
        for (auto& tunnel : socks5_tunnels_) {
            try {
                if (tunnel) {
                    tunnel->stop();
                }
            } catch (...) {
                // Игнорируем ошибки остановки туннелей
            }
        }
        socks5_tunnels_.clear();
        
    } catch (...) {
        // Игнорируем ошибки очистки туннелей
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
            int error = get_last_socket_error();
            if (is_temporary_error(error)) {
                break;
            }
            if (running_.load()) {
                Logger::error("Ошибка select в tunnel сервере: " + std::to_string(error));
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

        SOCKET tunnel_socket = accept(server_socket_, 
                                  reinterpret_cast<sockaddr*>(&client_addr), 
                                  &client_len);

        if (tunnel_socket == INVALID_SOCKET) {
            int error = get_last_socket_error();
            if (is_temporary_error(error)) {
                continue;
            }
            if (running_.load()) {
                Logger::error("Ошибка при принятии tunnel соединения: " + std::to_string(error));
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

void TunnelServer::handle_tunnel_connection(SOCKET tunnel_socket, 
                                          const std::string& client_ip, 
                                          int client_port) {
    try {
        // Читаем первый байт для определения протокола
        uint8_t first_byte;
        ssize_t received = recv(tunnel_socket, &first_byte, 1, MSG_PEEK);
        
        if (received <= 0) {
            Logger::error("Не удалось прочитать первый байт от " + 
                         client_ip + ":" + std::to_string(client_port));
            close(tunnel_socket);
            return;
        }
        
        route_to_handler(tunnel_socket, client_ip, client_port, first_byte);
        cleanup_finished_tunnels();
        
    } catch (const std::exception& e) {
        Logger::error("Ошибка обработки tunnel соединения: " + std::string(e.what()));
        close(tunnel_socket);
    }
}

void TunnelServer::route_to_handler(SOCKET tunnel_socket, const std::string& client_ip, 
                                  int client_port, uint8_t first_byte) {
    lock_guard_type<mutex_type> lock(tunnels_mutex_);
    
    int total_tunnels = static_cast<int>(http_tunnels_.size() + socks5_tunnels_.size());
    if (total_tunnels >= config_.get_max_connections()) {
        Logger::warning("Достигнут лимит tunnel соединений, отклонение " + 
                       client_ip + ":" + std::to_string(client_port));
        close(tunnel_socket);
        return;
    }
    
    // Определяем протокол по первому байту
    if (first_byte == 0x05) {
        // SOCKS5 протокол
        Logger::info("Обнаружен SOCKS5 клиент от " + client_ip + ":" + std::to_string(client_port));
        
        auto handler = std::make_shared<Socks5Handler>(static_cast<int>(tunnel_socket), client_ip, client_port, config_);
        
        if (handler->start()) {
            socks5_tunnels_.push_back(handler);
            Logger::info("SOCKS5 обработчик запущен для " + client_ip + ":" + std::to_string(client_port));
        } else {
            Logger::error("Не удалось запустить SOCKS5 обработчик для " + 
                         client_ip + ":" + std::to_string(client_port));
            close(tunnel_socket);
        }
        
    } else {
        // HTTP протокол (предполагаем, что всё остальное - HTTP)
        Logger::info("Обнаружен HTTP клиент от " + client_ip + ":" + std::to_string(client_port));
        
        auto handler = std::make_shared<ProxyHandler>(static_cast<int>(tunnel_socket), client_ip, client_port, config_);
        
        if (handler->start()) {
            http_tunnels_.push_back(handler);
            Logger::info("HTTP обработчик запущен для " + client_ip + ":" + std::to_string(client_port));
        } else {
            Logger::error("Не удалось запустить HTTP обработчик для " + 
                         client_ip + ":" + std::to_string(client_port));
            close(tunnel_socket);
        }
    }
}

void TunnelServer::cleanup_finished_tunnels() {
    lock_guard_type<mutex_type> lock(tunnels_mutex_);
    
    // Очистка HTTP туннелей
    auto http_it = http_tunnels_.begin();
    while (http_it != http_tunnels_.end()) {
        if (!(*http_it)->is_running()) {
            try {
                (*http_it)->stop();
                http_it = http_tunnels_.erase(http_it);
            } catch (...) {
                http_it = http_tunnels_.erase(http_it);
            }
        } else {
            ++http_it;
        }
    }
    
    // Очистка SOCKS5 туннелей
    auto socks5_it = socks5_tunnels_.begin();
    while (socks5_it != socks5_tunnels_.end()) {
        if (!(*socks5_it) || !(*socks5_it)->is_running()) {
            try {
                if (*socks5_it) {
                    (*socks5_it)->stop();
                }
                socks5_it = socks5_tunnels_.erase(socks5_it);
            } catch (...) {
                socks5_it = socks5_tunnels_.erase(socks5_it);
            }
        } else {
            ++socks5_it;
        }
    }
}

TunnelServer::TunnelStatus TunnelServer::get_status() const {
    lock_guard_type<mutex_type> lock(tunnels_mutex_);
    
    int http_count = static_cast<int>(http_tunnels_.size());
    int socks5_count = static_cast<int>(socks5_tunnels_.size());
    
    return TunnelStatus{
        running_.load(),
        http_count + socks5_count,
        http_count,
        socks5_count,
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