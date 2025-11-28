#include "socks5_handler.h"
#include "logger.h"
#include "utils.h"
#include <cstring>
#include <cerrno>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <thread>
#include <unistd.h>

Socks5Handler::Socks5Handler(int client_socket, const std::string& client_ip,
                           int client_port, const Config& config)
    : client_socket_(static_cast<SOCKET>(client_socket)), client_ip_(client_ip),
      client_port_(client_port), config_(config), tunnel_socket_(INVALID_SOCKET) {
    
    set_socket_timeout(client_socket_, config_.get_timeout());
    
    std::string key = config_.get_encryption_key();
    if (!encryption_manager_.load_encryption(
            config_.get_encryption_library(),
            reinterpret_cast<const unsigned char*>(key.c_str()),
            key.length())) {
        Logger::warning("Не удалось загрузить алгоритм шифрования для Socks5Handler");
    }
}

Socks5Handler::~Socks5Handler() noexcept {
    try {
        stop();
    } catch (...) {
        // Игнорируем исключения в деструкторе
    }
}

bool Socks5Handler::start() {
    if (running_.load()) {
        return false;
    }
    
    running_.store(true);
    handler_thread_ = threading::make_unique_thread(&Socks5Handler::handle, this);
    return true;
}

void Socks5Handler::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    // Закрываем сокеты чтобы прервать блокирующие операции
    if (client_socket_ != INVALID_SOCKET) {
        shutdown(client_socket_, SHUT_RDWR);
        close(client_socket_);
        client_socket_ = INVALID_SOCKET;
    }
    
    if (tunnel_socket_ != INVALID_SOCKET) {
        shutdown(tunnel_socket_, SHUT_RDWR);
        close(tunnel_socket_);
        tunnel_socket_ = INVALID_SOCKET;
    }
    
    // Ждем завершения потока
    if (handler_thread_ && handler_thread_->joinable()) {
        // Проверяем, что мы не в том же потоке
        if (handler_thread_->get_id() != std::this_thread::get_id()) {
            handler_thread_->join();
        } else {
            handler_thread_->detach();
        }
    }
}

void Socks5Handler::handle() {
    Logger::info("Новое SOCKS5 соединение от " + client_ip_ + ":" + std::to_string(client_port_));
    
    try {
        // Этап 1: Handshake
        if (!handle_handshake()) {
            Logger::error("Ошибка SOCKS5 handshake");
            return;
        }
        
        // Этап 2: Connect request
        std::string target_host;
        int target_port;
        if (!handle_connect_request(target_host, target_port)) {
            Logger::error("Ошибка SOCKS5 connect request");
            return;
        }
        
        // Этап 3: Подключение к туннелю
        if (!connect_to_tunnel(target_host, target_port)) {
            Logger::error("Ошибка подключения к туннелю для " + target_host + ":" + std::to_string(target_port));
            send_socks5_response(SOCKS5_REP_FAILURE);
            return;
        }
        
        // Отправляем успешный ответ
        send_socks5_response(SOCKS5_REP_SUCCESS);
        
        // Этап 4: Передача данных
        data_transfer();
        
    } catch (const std::exception& e) {
        Logger::error("Ошибка в SOCKS5 обработчике: " + std::string(e.what()));
    }
    
    stop();
}

bool Socks5Handler::handle_handshake() {
    // Читаем запрос аутентификации: [VERSION][NMETHODS][METHODS...]
    uint8_t buffer[257]; // Максимум 1 + 1 + 255
    
    ssize_t received = recv(client_socket_, buffer, 2, 0);
    if (received != 2) {
        Logger::error("Неверный размер SOCKS5 handshake");
        return false;
    }
    
    uint8_t version = buffer[0];
    uint8_t nmethods = buffer[1];
    
    if (version != SOCKS5_VERSION) {
        Logger::error("Неподдерживаемая версия SOCKS: " + std::to_string(version));
        return false;
    }
    
    if (nmethods == 0) {
        Logger::error("Нет методов аутентификации");
        return false;
    }
    
    // Читаем методы аутентификации
    received = recv(client_socket_, buffer, nmethods, 0);
    if (received != nmethods) {
        Logger::error("Не удалось прочитать методы аутентификации");
        return false;
    }
    
    // Проверяем, есть ли "без аутентификации" (0x00)
    bool no_auth_found = false;
    for (int i = 0; i < nmethods; i++) {
        if (buffer[i] == SOCKS5_NO_AUTH) {
            no_auth_found = true;
            break;
        }
    }
    
    // Отправляем ответ: [VERSION][METHOD]
    uint8_t auth_method = no_auth_found ? SOCKS5_NO_AUTH : 0xFF;
    uint8_t response[2] = {SOCKS5_VERSION, auth_method};
    ssize_t sent = send(client_socket_, response, 2, 0);
    
    if (sent != 2 || !no_auth_found) {
        Logger::error("SOCKS5 аутентификация отклонена");
        return false;
    }
    
    Logger::info("SOCKS5 handshake успешен");
    return true;
}

bool Socks5Handler::handle_connect_request(std::string& target_host, int& target_port) {
    // Читаем запрос: [VER][CMD][RSV][ATYP][DST.ADDR][DST.PORT]
    uint8_t buffer[512];
    
    ssize_t received = recv(client_socket_, buffer, 4, 0);
    if (received != 4) {
        Logger::error("Неверный размер SOCKS5 connect request");
        return false;
    }
    
    uint8_t version = buffer[0];
    uint8_t cmd = buffer[1];
    uint8_t atyp = buffer[3]; // buffer[2] зарезервирован
    
    if (version != SOCKS5_VERSION) {
        Logger::error("Неверная версия в connect request: " + std::to_string(version));
        return false;
    }
    
    if (cmd != SOCKS5_CMD_CONNECT) {
        Logger::error("Неподдерживаемая команда SOCKS5: " + std::to_string(cmd));
        return false;
    }
    
    // Читаем адрес назначения
    if (atyp == SOCKS5_ATYP_IPV4) {
        // IPv4: 4 байта IP + 2 байта порт
        received = recv(client_socket_, buffer, 6, 0);
        if (received != 6) {
            Logger::error("Не удалось прочитать IPv4 адрес");
            return false;
        }
        
        struct in_addr addr;
        memcpy(&addr, buffer, 4);
        target_host = inet_ntoa(addr);
        target_port = ntohs(*reinterpret_cast<uint16_t*>(buffer + 4));
        
    } else if (atyp == SOCKS5_ATYP_DOMAIN) {
        // Домен: 1 байт длина + домен + 2 байта порт
        received = recv(client_socket_, buffer, 1, 0);
        if (received != 1) {
            Logger::error("Не удалось прочитать длину домена");
            return false;
        }
        
        uint8_t domain_len = buffer[0];
        if (domain_len == 0) {
            Logger::error("Пустое доменное имя");
            return false;
        }
        
        received = recv(client_socket_, buffer, domain_len + 2, 0);
        if (received != domain_len + 2) {
            Logger::error("Не удалось прочитать доменное имя и порт");
            return false;
        }
        
        target_host = std::string(reinterpret_cast<char*>(buffer), domain_len);
        target_port = ntohs(*reinterpret_cast<uint16_t*>(buffer + domain_len));
        
    } else {
        Logger::error("Неподдерживаемый тип адреса: " + std::to_string(atyp));
        return false;
    }
    
    Logger::info("SOCKS5 запрос к " + target_host + ":" + std::to_string(target_port));
    return true;
}

bool Socks5Handler::connect_to_tunnel(const std::string& target_host, int target_port) {
    // Создаем подключение к нашему VPN серверу
    tunnel_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (tunnel_socket_ == INVALID_SOCKET) {
        Logger::error("Не удалось создать tunnel socket");
        return false;
    }
    
    set_socket_timeout(tunnel_socket_, config_.get_timeout());
    
    struct sockaddr_in tunnel_addr;
    memset(&tunnel_addr, 0, sizeof(tunnel_addr));
    tunnel_addr.sin_family = AF_INET;
    tunnel_addr.sin_port = htons(config_.get_server_port());
    
    if (inet_pton(AF_INET, config_.get_server_host().c_str(), &tunnel_addr.sin_addr) <= 0) {
        Logger::error("Неверный адрес VPN сервера");
        return false;
    }
    
    if (connect(tunnel_socket_, reinterpret_cast<sockaddr*>(&tunnel_addr), sizeof(tunnel_addr)) < 0) {
        Logger::error("Не удалось подключиться к VPN серверу");
        return false;
    }
    
    Logger::info("Подключен к VPN серверу для " + target_host + ":" + std::to_string(target_port));
    
    // Отправляем зашифрованную информацию о цели в правильном формате
    // Формат: длина_хоста (4 байта) + хост + порт (2 байта)
    
    uint32_t host_len = target_host.length();
    uint32_t host_len_be = htonl(host_len);
    uint16_t port_be = htons(target_port);
    
    // Шифруем длину хоста
    encryption_manager_.encrypt(reinterpret_cast<unsigned char*>(&host_len_be), sizeof(host_len_be));
    
    if (send(tunnel_socket_, &host_len_be, sizeof(host_len_be), 0) != sizeof(host_len_be)) {
        Logger::error("Не удалось отправить длину хоста");
        return false;
    }
    
    // Шифруем и отправляем хост
    std::vector<char> encrypted_host(target_host.begin(), target_host.end());
    encryption_manager_.encrypt(reinterpret_cast<unsigned char*>(encrypted_host.data()), encrypted_host.size());
    
    if (send(tunnel_socket_, encrypted_host.data(), encrypted_host.size(), 0) != static_cast<ssize_t>(encrypted_host.size())) {
        Logger::error("Не удалось отправить хост");
        return false;
    }
    
    // Шифруем и отправляем порт
    encryption_manager_.encrypt(reinterpret_cast<unsigned char*>(&port_be), sizeof(port_be));
    
    if (send(tunnel_socket_, &port_be, sizeof(port_be), 0) != sizeof(port_be)) {
        Logger::error("Не удалось отправить порт");
        return false;
    }
    
    Logger::info("Информация о цели отправлена через туннель");
    return true;
}

void Socks5Handler::send_socks5_response(uint8_t reply_code, const std::string& bind_addr, uint16_t bind_port) {
    // Ответ: [VER][REP][RSV][ATYP][BND.ADDR][BND.PORT]
    uint8_t response[10]; // Максимум для IPv4
    
    response[0] = SOCKS5_VERSION;
    response[1] = reply_code;
    response[2] = 0x00; // Зарезервировано
    response[3] = SOCKS5_ATYP_IPV4;
    
    // Bind address (обычно 0.0.0.0)
    inet_pton(AF_INET, bind_addr.c_str(), &response[4]);
    
    // Bind port
    uint16_t port_be = htons(bind_port);
    memcpy(&response[8], &port_be, 2);
    
    ssize_t sent = send(client_socket_, response, 10, 0);
    if (sent != 10) {
        Logger::error("Не удалось отправить SOCKS5 ответ");
    } else {
        Logger::info("SOCKS5 ответ отправлен: " + std::to_string(reply_code));
    }
}

void Socks5Handler::data_transfer() {
    Logger::info("Начинаем передачу данных SOCKS5");
    
    fd_set read_fds;
    int max_fd = std::max(static_cast<int>(client_socket_), static_cast<int>(tunnel_socket_));
    
    while (running_.load()) {
        FD_ZERO(&read_fds);
        FD_SET(client_socket_, &read_fds);
        FD_SET(tunnel_socket_, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (ready <= 0) {
            if (ready < 0 && errno != EINTR && errno != EBADF) {
                Logger::error("Ошибка select в SOCKS5 передаче данных: " + std::to_string(errno));
                break;
            }
            continue;
        }
        
        // Данные от клиента к туннелю
        if (FD_ISSET(client_socket_, &read_fds) && running_.load()) {
            char buffer[4096];
            ssize_t received = recv(client_socket_, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                if (received < 0) {
                    Logger::error("Ошибка чтения от клиента SOCKS5: " + std::to_string(errno));
                } else {
                    Logger::info("Клиент SOCKS5 закрыл соединение");
                }
                break;
            }
            
            // Шифруем и отправляем через туннель
            encryption_manager_.encrypt(reinterpret_cast<unsigned char*>(buffer), received);
            ssize_t sent = send(tunnel_socket_, buffer, received, 0);
            if (sent != received) {
                Logger::error("Ошибка отправки данных в туннель: " + std::to_string(errno));
                break;
            }
        }
        
        // Данные от туннеля к клиенту
        if (FD_ISSET(tunnel_socket_, &read_fds) && running_.load()) {
            char buffer[4096];
            ssize_t received = recv(tunnel_socket_, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                if (received < 0) {
                    Logger::error("Ошибка чтения от туннеля SOCKS5: " + std::to_string(errno));
                } else {
                    Logger::info("Туннель SOCKS5 закрыл соединение");
                }
                break;
            }
            
            // Расшифровываем и отправляем клиенту
            encryption_manager_.decrypt(reinterpret_cast<unsigned char*>(buffer), received);
            ssize_t sent = send(client_socket_, buffer, received, 0);
            if (sent != received) {
                Logger::error("Ошибка отправки данных клиенту: " + std::to_string(errno));
                break;
            }
        }
    }
    
    Logger::info("Передача данных SOCKS5 завершена");
}