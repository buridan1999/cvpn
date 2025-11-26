#include "tunnel_handler.h"
#include "logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <cstring>
#include <netdb.h>
#include <cerrno>

TunnelHandler::TunnelHandler(int tunnel_socket, const std::string& client_ip,
                           int client_port, const Config& config)
    : config_(config), tunnel_socket_(tunnel_socket), 
      client_ip_(client_ip), client_port_(client_port) {
    
    // Настройка таймаута
    struct timeval timeout;
    timeout.tv_sec = config_.get_timeout();
    timeout.tv_usec = 0;
    
    setsockopt(tunnel_socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(tunnel_socket_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

TunnelHandler::~TunnelHandler() noexcept {
    try {
        stop();
    } catch (...) {
        // Игнорируем исключения в деструкторе
    }
}

bool TunnelHandler::start() {
    if (running_.load()) {
        return false;
    }

    running_.store(true);
    handler_thread_ = std::make_unique<std::thread>(&TunnelHandler::handle, this);
    
    return true;
}

void TunnelHandler::stop() {
    running_.store(false);

    if (tunnel_socket_ >= 0) {
        close(tunnel_socket_);
        tunnel_socket_ = -1;
    }
    
    if (target_socket_ >= 0) {
        close(target_socket_);
        target_socket_ = -1;
    }

    if (handler_thread_ && handler_thread_->joinable()) {
        handler_thread_->join();
    }
    
    if (to_target_thread_ && to_target_thread_->joinable()) {
        to_target_thread_->join();
    }
    
    if (from_target_thread_ && from_target_thread_->joinable()) {
        from_target_thread_->join();
    }
}

void TunnelHandler::handle() {
    std::string target_host;
    int target_port;
    std::vector<char> initial_data;
    
    // Читаем мутированные данные от прокси-сервера
    if (!read_mutated_data(target_host, target_port, initial_data)) {
        Logger::error("Не удалось прочитать мутированные данные от прокси сервера");
        return;
    }

    // Подключаемся к целевому серверу
    if (!connect_to_target(target_host, target_port)) {
        Logger::error("Не удалось подключиться к целевому серверу " + target_host + 
                     ":" + std::to_string(target_port));
        return;
    }

    Logger::info("Установлен туннель: " + client_ip_ + ":" + std::to_string(client_port_) + 
                " -> " + target_host + ":" + std::to_string(target_port));

    // Начинаем передачу данных
    start_data_transfer(initial_data);
    
    running_.store(false);
}

bool TunnelHandler::read_mutated_data(std::string& target_host, int& target_port, 
                                     std::vector<char>& initial_data) {
    try {
        // Читаем заголовок: 4 байта длина хоста + хост + 2 байта порт
        char buffer[4];
        
        Logger::info("Ожидаем данные от прокси сервера...");
        
        // Читаем длину хоста
        ssize_t received = recv(tunnel_socket_, buffer, 4, MSG_WAITALL);
        if (received != 4) {
            Logger::error("Не удалось прочитать длину хоста, получено: " + std::to_string(received));
            if (received > 0) {
                // Показываем что получили в hex
                std::string hex_data;
                for (int i = 0; i < received; i++) {
                    char hex[3];
                    sprintf(hex, "%02x", (unsigned char)buffer[i]);
                    hex_data += hex;
                }
                Logger::error("Получены байты (hex): " + hex_data);
            }
            return false;
        }
        
        // Показываем сырые данные перед демутацией
        std::string hex_data;
        for (int i = 0; i < 4; i++) {
            char hex[3];
            sprintf(hex, "%02x", (unsigned char)buffer[i]);
            hex_data += hex;
        }
        Logger::info("Получена длина хоста (hex до демутации): " + hex_data);
        
        // Демутируем длину хоста
        decrypt(buffer, 4);
        
        // Показываем демутированные данные
        hex_data.clear();
        for (int i = 0; i < 4; i++) {
            char hex[3];
            sprintf(hex, "%02x", (unsigned char)buffer[i]);
            hex_data += hex;
        }
        Logger::info("Длина хоста после демутации (hex): " + hex_data);
        
        uint32_t host_len;
        memcpy(&host_len, buffer, 4);
        host_len = ntohl(host_len); // Преобразуем из сетевого порядка байтов
        
        Logger::info("Демутированная длина хоста: " + std::to_string(host_len));
        
        if (host_len > 255 || host_len == 0) {
            Logger::error("Некорректная длина хоста: " + std::to_string(host_len));
            return false;
        }
        
        // Читаем хост
        std::vector<char> host_buffer(host_len);
        received = recv(tunnel_socket_, host_buffer.data(), host_len, MSG_WAITALL);
        if (received != static_cast<ssize_t>(host_len)) {
            Logger::error("Не удалось прочитать хост, ожидали: " + std::to_string(host_len) + 
                         ", получили: " + std::to_string(received));
            return false;
        }
        
        // Демутируем хост
        decrypt(host_buffer.data(), host_len);
        target_host = std::string(host_buffer.data(), host_len);
        
        // Читаем порт
        received = recv(tunnel_socket_, buffer, 2, MSG_WAITALL);
        if (received != 2) {
            Logger::error("Не удалось прочитать порт, получено: " + std::to_string(received));
            return false;
        }
        
        // Демутируем порт
        decrypt(buffer, 2);
        
        uint16_t port;
        memcpy(&port, buffer, 2);
        target_port = ntohs(port); // Преобразуем из сетевого порядка байтов
        
        Logger::info("Успешно демутированы данные: " + target_host + ":" + std::to_string(target_port));
        
        // Читаем остальные данные (если есть) - с небольшим таймаутом
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(tunnel_socket_, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms
        
        int ready = select(tunnel_socket_ + 1, &read_fds, nullptr, nullptr, &timeout);
        if (ready > 0 && FD_ISSET(tunnel_socket_, &read_fds)) {
            char data_buffer[4096];
            received = recv(tunnel_socket_, data_buffer, sizeof(data_buffer), 0);
            if (received > 0) {
                // Демутируем начальные данные
                decrypt(data_buffer, received);
                initial_data.assign(data_buffer, data_buffer + received);
                Logger::info("Получены начальные данные: " + std::to_string(received) + " байт");
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Ошибка при чтении мутированных данных: " + std::string(e.what()));
        return false;
    }
}

bool TunnelHandler::connect_to_target(const std::string& host, int port) {
    Logger::info("Туннель подключается к " + host + ":" + std::to_string(port));
    
    target_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (target_socket_ < 0) {
        Logger::error("Не удалось создать сокет для целевого сервера");
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    
    setsockopt(target_socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(target_socket_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    sockaddr_in target_addr{};
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &target_addr.sin_addr) > 0) {
        Logger::info("Используется IP адрес: " + host);
    } else {
        Logger::info("Резолвим домен: " + host);
        struct hostent* host_entry = gethostbyname(host.c_str());
        if (!host_entry) {
            Logger::error("DNS резолв не удался для " + host);
            close(target_socket_);
            target_socket_ = -1;
            return false;
        }
        
        char* ip_str = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
        Logger::info("DNS резолв: " + host + " -> " + std::string(ip_str));
        memcpy(&target_addr.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
    }

    if (connect(target_socket_, reinterpret_cast<sockaddr*>(&target_addr), 
                sizeof(target_addr)) < 0) {
        Logger::error("Не удалось подключиться к " + host + ":" + 
                     std::to_string(port) + " - " + strerror(errno));
        close(target_socket_);
        target_socket_ = -1;
        return false;
    }

    Logger::info("Успешно подключились к " + host + ":" + std::to_string(port));
    return true;
}

void TunnelHandler::start_data_transfer(const std::vector<char>& initial_data) {
    Logger::info("Начинаем передачу данных через туннель");
    
    // Если есть начальные данные, отправляем их на целевой сервер
    if (!initial_data.empty()) {
        if (send(target_socket_, initial_data.data(), initial_data.size(), 0) < 0) {
            Logger::error("Не удалось отправить начальные данные");
            return;
        }
        Logger::info("Отправлены начальные данные: " + std::to_string(initial_data.size()) + " байт");
    }
    
    // Запускаем потоки для двунаправленной передачи
    to_target_thread_ = std::make_unique<std::thread>(
        &TunnelHandler::transfer_data_to_target, this, tunnel_socket_, target_socket_);
    
    from_target_thread_ = std::make_unique<std::thread>(
        &TunnelHandler::transfer_data_from_target, this, target_socket_, tunnel_socket_);
    
    // Ожидаем завершения любого из потоков
    if (to_target_thread_->joinable()) {
        to_target_thread_->join();
    }
    if (from_target_thread_->joinable()) {
        from_target_thread_->join();
    }
    
    Logger::info("Передача данных через туннель завершена");
}

void TunnelHandler::transfer_data_to_target(int source_socket, int destination_socket) {
    char buffer[4096];
    
    while (running_.load()) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(source_socket, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ready = select(source_socket + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (ready <= 0) {
            if (ready < 0 && errno != EINTR) {
                Logger::error("Ошибка select в передаче к цели");
                break;
            }
            continue;
        }
        
        if (!FD_ISSET(source_socket, &read_fds)) {
            continue;
        }
        
        ssize_t received = recv(source_socket, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            if (received < 0) {
                Logger::error("Ошибка чтения от туннеля: " + std::string(strerror(errno)));
            } else {
                Logger::info("Туннельное соединение закрыто");
            }
            break;
        }
        
        // Демутируем данные перед отправкой на целевой сервер
        decrypt(buffer, received);
        
        if (send(destination_socket, buffer, received, 0) != received) {
            Logger::error("Ошибка отправки к целевому серверу");
            break;
        }
    }
    
    running_.store(false);
    Logger::info("Передача данных к цели завершена");
}

void TunnelHandler::transfer_data_from_target(int source_socket, int destination_socket) {
    char buffer[4096];
    
    while (running_.load()) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(source_socket, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ready = select(source_socket + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (ready <= 0) {
            if (ready < 0 && errno != EINTR) {
                Logger::error("Ошибка select в передаче от цели");
                break;
            }
            continue;
        }
        
        if (!FD_ISSET(source_socket, &read_fds)) {
            continue;
        }
        
        ssize_t received = recv(source_socket, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            if (received < 0) {
                Logger::error("Ошибка чтения от целевого сервера: " + std::string(strerror(errno)));
            } else {
                Logger::info("Целевой сервер закрыл соединение");
            }
            break;
        }
        
        // Мутируем данные перед отправкой обратно в туннель
        encrypt(buffer, received);
        
        if (send(destination_socket, buffer, received, 0) != received) {
            Logger::error("Ошибка отправки обратно в туннель");
            break;
        }
    }
    
    running_.store(false);
    Logger::info("Передача данных от цели завершена");
}

void TunnelHandler::decrypt(char* data, size_t size) {
    unsigned char key = config_.get_xor_key();
    for (size_t i = 0; i < size; ++i) {
        data[i] ^= key;
    }
}

void TunnelHandler::encrypt(char* data, size_t size) {
    unsigned char key = config_.get_xor_key();
    for (size_t i = 0; i < size; ++i) {
        data[i] ^= key;
    }
}