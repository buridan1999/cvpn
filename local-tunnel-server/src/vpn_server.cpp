#include "vpn_server.h"
#include "logger.h"
#include "utils.h"
#include "platform_compat.h"
#include <csignal>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <cerrno>
#include <chrono>
#include <iomanip>
#include <sstream>

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// Статический указатель для обработки сигналов
VPNServer* VPNServer::instance_ = nullptr;

VPNServer::VPNServer(const Config& config) 
    : config_(config) {
    
    // Установка обработчика сигналов
    instance_ = this;
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::cout << get_timestamp() << " [INFO] VPN сервер инициализирован" << std::endl;
}

VPNServer::~VPNServer() {
    stop();
    instance_ = nullptr;
}

bool VPNServer::start() {
    if (running_.load()) {
        Logger::warning("Сервер уже запущен");
        return false;
    }

    // Создание серверного сокета
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == INVALID_SOCKET) {
        int error = get_last_socket_error();
        Logger::error("Не удалось создать серверный сокет: " + std::to_string(error));
        return false;
    }

    // Настройка сокета для повторного использования адреса
    if (set_socket_reuse(server_socket_) != 0) {
        int error = get_last_socket_error();
        Logger::error("Не удалось настроить SO_REUSEADDR: " + std::to_string(error));
        close(server_socket_);
        return false;
    }

    // Настройка адреса сервера
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config_.get_server_port());
    
    if (config_.get_server_host() == "0.0.0.0") {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton_compat(AF_INET, config_.get_server_host().c_str(), &server_addr.sin_addr) <= 0) {
            Logger::error("Некорректный IP адрес сервера: " + config_.get_server_host());
            close(server_socket_);
            return false;
        }
    }

    // Привязка сокета к адресу
    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) != 0) {
        int error = get_last_socket_error();
        Logger::error("Не удалось привязать сокет к адресу " + 
                     config_.get_server_host() + ":" + std::to_string(config_.get_server_port()) +
                     ", ошибка: " + std::to_string(error));
        close(server_socket_);
        return false;
    }

    // Начало прослушивания
    if (listen(server_socket_, config_.get_max_connections()) != 0) {
        int error = get_last_socket_error();
        Logger::error("Не удалось начать прослушивание сокета: " + std::to_string(error));
        close(server_socket_);
        return false;
    }

    running_.store(true);
    
    // Запуск серверного потока
    server_thread_ = threading::make_unique_thread(&VPNServer::server_loop, this);

    Logger::info("VPN сервер запущен на " + config_.get_server_host() + 
                ":" + std::to_string(config_.get_server_port()));
    Logger::info("Максимальное количество соединений: " + 
                std::to_string(config_.get_max_connections()));

    return true;
}

void VPNServer::stop() {
    if (!running_.load()) {
        return;
    }

    Logger::info("Остановка VPN сервера...");
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

    // Закрытие всех клиентских соединений
    try {
        lock_guard_type<mutex_type> lock(clients_mutex_);
        for (auto& client : clients_) {
            try {
                if (client) {
                    client->stop();
                }
            } catch (...) {
                // Игнорируем ошибки остановки клиентов
            }
        }
        clients_.clear();
    } catch (...) {
        // Игнорируем ошибки очистки клиентов
    }

    Logger::info("VPN сервер остановлен");
}

void VPNServer::server_loop() {
    while (running_.load()) {
        // Используем select для неблокирующего ожидания соединений
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket_, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;  // Проверяем каждую секунду
        timeout.tv_usec = 0;
        
        int ready = select(server_socket_ + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (ready < 0) {
            int error = get_last_socket_error();
            if (is_temporary_error(error)) {
                Logger::info("Получен сигнал прерывания");
                break;
            }
            if (running_.load()) {
                Logger::error("Ошибка select: " + std::to_string(error));
            }
            continue;
        }
        
        if (ready == 0) {
            // Таймаут - очищаем клиентов и продолжаем
            cleanup_finished_clients();
            continue;
        }
        
        if (!FD_ISSET(server_socket_, &read_fds)) {
            continue;
        }

        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        // Принятие нового соединения
        SOCKET client_socket = accept(server_socket_, 
                                  reinterpret_cast<sockaddr*>(&client_addr), 
                                  &client_len);

        if (client_socket == INVALID_SOCKET) {
            int error = get_last_socket_error();
            if (is_temporary_error(error)) {
                continue;
            }
            if (running_.load()) {
                Logger::error("Ошибка при принятии соединения: " + std::to_string(error));
            }
            continue;
        }

        // Получение информации о клиенте
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        Logger::info("Новое соединение от " + std::string(client_ip) + 
                    ":" + std::to_string(client_port));

        // Обработка клиентского соединения
        handle_client_connection(client_socket, client_ip, client_port);

        // Очистка завершенных клиентских соединений
        cleanup_finished_clients();
    }
    
    Logger::info("Выход из основного цикла сервера");
}

void VPNServer::handle_client_connection(SOCKET client_socket, 
                                       const std::string& client_ip, 
                                       int client_port) {
    
    try {
        // Проверка лимита соединений
        {
            lock_guard_type<mutex_type> lock(clients_mutex_);
            if (static_cast<int>(clients_.size()) >= config_.get_max_connections()) {
                Logger::warning("Достигнут лимит соединений, отклонение клиента " + 
                               client_ip + ":" + std::to_string(client_port));
                close(client_socket);
                return;
            }
        }

        // Создание обработчика для клиента
        auto handler = std::make_shared<ProxyHandler>(static_cast<int>(client_socket), client_ip, client_port, config_);
        
        // Запуск обработчика
        if (handler->start()) {
            // Добавление в список активных клиентов только если запуск успешен
            lock_guard_type<mutex_type> lock(clients_mutex_);
            clients_.push_back(handler);
        } else {
            Logger::error("Не удалось запустить обработчик для клиента " + 
                         client_ip + ":" + std::to_string(client_port));
            close(client_socket);
        }
    } catch (const std::exception& e) {
        Logger::error("Ошибка обработки клиентского соединения: " + std::string(e.what()));
        close(client_socket);
    } catch (...) {
        Logger::error("Неизвестная ошибка обработки клиентского соединения");
        close(client_socket);
    }
}

void VPNServer::cleanup_finished_clients() {
    lock_guard_type<mutex_type> lock(clients_mutex_);
    
    auto it = clients_.begin();
    while (it != clients_.end()) {
        if (!(*it)->is_running()) {
            try {
                (*it)->stop(); // Убеждаемся, что полностью остановлен
                it = clients_.erase(it);
            } catch (...) {
                // Игнорируем ошибки при cleanup
                it = clients_.erase(it);
            }
        } else {
            ++it;
        }
    }
}

VPNServer::ServerStatus VPNServer::get_status() const {
    lock_guard_type<mutex_type> lock(clients_mutex_);
    
    return ServerStatus{
        running_.load(),
        static_cast<int>(clients_.size()),
        config_.get_server_host(),
        config_.get_server_port()
    };
}

void VPNServer::signal_handler(int signal) {
    if (instance_) {
        Logger::info("Получен сигнал " + std::to_string(signal) + 
                    ", завершение работы сервера...");
        instance_->stop();
    }
}