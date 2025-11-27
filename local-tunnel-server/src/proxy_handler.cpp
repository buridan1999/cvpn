#include "proxy_handler.h"
#include "logger.h"
#include "utils.h"
#include "platform_compat.h"
#include <cstring>
#include <cstdint>
#include <sstream>
#include <vector>
#include <cerrno>
#include <chrono>
#include <thread>

ProxyHandler::ProxyHandler(int client_socket, const std::string& client_ip,
                          int client_port, const Config& config)
    : client_socket_(static_cast<SOCKET>(client_socket)), client_ip_(client_ip),
      client_port_(client_port), config_(config) {
    
    // Настройка таймаута для клиентского сокета
    set_socket_timeout(client_socket_, config_.get_timeout());
    
    // Инициализация менеджера шифрования
    std::string key = config_.get_encryption_key();
    if (!encryption_manager_.load_encryption(
            config_.get_encryption_library(),
            reinterpret_cast<const unsigned char*>(key.c_str()),
            key.length())) {
        Logger::warning("Не удалось загрузить алгоритм шифрования для ProxyHandler");
    }
}

ProxyHandler::~ProxyHandler() noexcept {
    try {
        stop();
    } catch (...) {
        // Игнорируем исключения в деструкторе
    }
}

bool ProxyHandler::start() {
    if (running_.load()) {
        return false;
    }

    running_.store(true);
    
    // Запуск основного потока обработчика
    handler_thread_ = threading::make_unique_thread(&ProxyHandler::handle, this);
    
    return true;
}

void ProxyHandler::stop() {
    running_.store(false);

    // Простое закрытие сокетов
    if (client_socket_ != INVALID_SOCKET) {
        close(client_socket_);
        client_socket_ = INVALID_SOCKET;
    }
    
    if (tunnel_socket_ != INVALID_SOCKET) {
        close(tunnel_socket_);
        tunnel_socket_ = INVALID_SOCKET;
    }

    // Ожидание завершения основного потока
    if (handler_thread_ && handler_thread_->joinable()) {
        handler_thread_->join();
    }
}

void ProxyHandler::handle() {
    std::string target_host;
    int target_port;
    
    if (!get_target_info(target_host, target_port)) {
        Logger::error("Не удалось получить информацию о целевом сервере от " + 
                     client_ip_ + ":" + std::to_string(client_port_));
        return;
    }

    // Сохраняем информацию о цели
    target_host_ = target_host;
    target_port_ = target_port;

    if (!connect_to_tunnel(target_host, target_port)) {
        Logger::error("Не удалось подключиться к туннелю для " + target_host + 
                     ":" + std::to_string(target_port));
        send_connection_response(false);
        return;
    }

    Logger::info("Установлен прокси туннель: " + client_ip_ + 
                ":" + std::to_string(client_port_) + 
                " -> TUNNEL -> " + target_host + ":" + std::to_string(target_port));

    send_connection_response(true);

    // Отправляем информацию о цели в туннель после успешного подключения
    send_mutated_target_info(target_host_, target_port_);

    if (!is_http_connect_) {
        forward_http_request();
    }

    start_data_transfer();
    
    running_.store(false);
}

bool ProxyHandler::get_target_info(std::string& target_host, int& target_port) {
    try {
        // Проверяем валидность сокета
        if (client_socket_ == INVALID_SOCKET) {
            Logger::error("Клиентский сокет не валиден");
            return false;
        }

        // Читаем первую строку для определения протокола
        std::string first_line;
        char buffer[1024];
        int pos = 0;
        
        // Читаем символ за символом до \r\n с таймаутом
        while (pos < sizeof(buffer) - 1) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(client_socket_, &read_fds);
            
            struct timeval timeout;
            timeout.tv_sec = 5;  // 5 секунд на чтение заголовка
            timeout.tv_usec = 0;
            
            int ready = select(client_socket_ + 1, &read_fds, nullptr, nullptr, &timeout);
            if (ready <= 0) {
                Logger::error("Таймаут или ошибка при чтении заголовка");
                return false;
            }
            
            ssize_t received = recv(client_socket_, &buffer[pos], 1, 0);
            if (received <= 0) {
                if (received == 0) {
                    Logger::info("Соединение закрыто клиентом при чтении заголовка");
                } else {
                    int error = get_last_socket_error();
                    Logger::error("Ошибка чтения данных: " + std::to_string(error));
                }
                return false;
            }
            
            if (buffer[pos] == '\n') {
                if (pos > 0 && buffer[pos-1] == '\r') {
                    buffer[pos-1] = '\0'; // Убираем \r\n
                    first_line = std::string(buffer);
                    break;
                }
            }
            pos++;
        }
        
        if (first_line.empty()) {
            Logger::warning("Получена пустая первая строка");
            return false;
        }
        
        Logger::info("Получена первая строка: " + first_line);
        
        // Проверяем тип HTTP запроса
        if (first_line.find("CONNECT ") == 0) {
            return parse_http_connect(first_line, target_host, target_port);
        } else if (first_line.find("GET ") == 0 || first_line.find("POST ") == 0 || 
                  first_line.find("PUT ") == 0 || first_line.find("DELETE ") == 0) {
            return parse_http_request(first_line, target_host, target_port);
        } else {
            // Предполагаем, что это наш бинарный протокол
            // Но первые байты уже прочитаны как строка, поэтому нужно их обработать
            return parse_binary_protocol_from_buffer(buffer, pos + 1, target_host, target_port);
        }

    } catch (const std::exception& e) {
        Logger::error("Ошибка при получении информации о целевом сервере: " + 
                     std::string(e.what()));
        return false;
    } catch (...) {
        Logger::error("Неизвестная ошибка при получении информации о целевом сервере");
        return false;
    }
}

ssize_t ProxyHandler::recv_exact(SOCKET socket, void* buffer, size_t size) {
    size_t total_received = 0;
    char* buf = static_cast<char*>(buffer);

    while (total_received < size && running_.load()) {
        ssize_t received = recv(socket, buf + total_received, size - total_received, 0);
        
        if (received <= 0) {
            if (received == 0) {
                Logger::info("Соединение закрыто клиентом");
            } else {
                int error = get_last_socket_error();
                Logger::error("Ошибка при получении данных: " + std::to_string(error));
            }
            return received;
        }
        
        total_received += received;
    }

    return total_received;
}

bool ProxyHandler::connect_to_tunnel(const std::string& target_host, int target_port) {
    Logger::info("Подключение к туннелю для " + target_host + ":" + std::to_string(target_port));
    
    // Создание сокета для туннеля
    tunnel_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (tunnel_socket_ == INVALID_SOCKET) {
        int error = get_last_socket_error();
        Logger::error("Не удалось создать сокет для туннеля: " + std::to_string(error));
        return false;
    }

    // Настройка таймаута
    set_socket_timeout(tunnel_socket_, 10);

    // Подключение к туннельному серверу
    sockaddr_in tunnel_addr{};
    tunnel_addr.sin_family = AF_INET;
    tunnel_addr.sin_port = htons(config_.get_server_port());
    
    if (inet_pton(AF_INET, config_.get_server_host().c_str(), &tunnel_addr.sin_addr) <= 0) {
        Logger::error("Некорректный IP адрес сервера: " + config_.get_server_host());
        close(tunnel_socket_);
        tunnel_socket_ = -1;
        return false;
    }

    Logger::info("Подключаюсь к серверу " + config_.get_server_host() + 
                ":" + std::to_string(config_.get_server_port()));

    if (connect(tunnel_socket_, reinterpret_cast<sockaddr*>(&tunnel_addr), 
                sizeof(tunnel_addr)) != 0) {
        int error = get_last_socket_error();
        Logger::error("Не удалось подключиться к туннелю: " + std::to_string(error));
        close(tunnel_socket_);
        tunnel_socket_ = INVALID_SOCKET;
        return false;
    }

    Logger::info("Успешно подключились к туннелю");
    
    // НЕ отправляем информацию о цели здесь - она будет отправлена позже
    
    return true;
}

void ProxyHandler::send_connection_response(bool success) {
    try {
        if (is_http_connect_) {
            // Для CONNECT запросов отправляем HTTP ответ
            send_http_response(success);
        } else if (!success) {
            // Для обычных HTTP запросов отправляем ошибку только при неудаче
            std::string error_response = "HTTP/1.1 502 Bad Gateway\r\n"
                                       "Content-Type: text/html\r\n"
                                       "Content-Length: 54\r\n"
                                       "\r\n"
                                       "<html><body><h1>502 Bad Gateway</h1></body></html>";
            send(client_socket_, error_response.c_str(), error_response.length(), 0);
        }
        // Для обычных HTTP запросов при успехе не отправляем ответ здесь - 
        // данные будут переданы напрямую от целевого сервера
    } catch (const std::exception& e) {
        Logger::error("Ошибка при отправке ответа клиенту: " + std::string(e.what()));
    }
}

void ProxyHandler::start_data_transfer() {
    Logger::info("Начинаем передачу данных через туннель");
    
    fd_set read_fds;
    char buffer[4096];
    
    while (running_.load()) {
        FD_ZERO(&read_fds);
        FD_SET(client_socket_, &read_fds);
        FD_SET(tunnel_socket_, &read_fds);
        
        int max_fd = std::max(client_socket_, tunnel_socket_);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (ready < 0) {
            if (errno == EINTR) continue;
            Logger::error("Ошибка select: " + std::string(strerror(errno)));
            break;
        }
        
        if (ready == 0) continue;
        
        // Передача от клиента к туннелю (с мутацией)
        if (FD_ISSET(client_socket_, &read_fds)) {
            ssize_t received = recv(client_socket_, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                if (received < 0) {
                    Logger::error("Ошибка чтения от клиента: " + std::string(strerror(errno)));
                } else {
                    Logger::info("Клиент закрыл соединение");
                }
                break;
            }
            
            // Мутируем данные перед отправкой в туннель
            encrypt(reinterpret_cast<unsigned char*>(buffer), received);
            
            if (send(tunnel_socket_, buffer, received, 0) != received) {
                Logger::error("Ошибка отправки в туннель");
                break;
            }
        }
        
        // Передача от туннеля к клиенту (с демутацией)
        if (FD_ISSET(tunnel_socket_, &read_fds)) {
            ssize_t received = recv(tunnel_socket_, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                if (received < 0) {
                    Logger::error("Ошибка чтения от туннеля: " + std::string(strerror(errno)));
                } else {
                    Logger::info("Туннель закрыл соединение");
                }
                break;
            }
            
            // Демутируем данные перед отправкой клиенту
            encrypt(reinterpret_cast<unsigned char*>(buffer), received);
            
            if (send(client_socket_, buffer, received, 0) != received) {
                Logger::error("Ошибка отправки к клиенту");
                break;
            }
        }
    }
    
    Logger::info("Передача данных завершена");
}

void ProxyHandler::transfer_data(SOCKET source_socket, SOCKET destination_socket,
                                const std::string& direction) {
    std::vector<char> buffer(config_.get_buffer_size());
    size_t total_bytes = 0;
    
    try {
        while (running_.load()) {
            // Проверяем валидность сокетов
            if (source_socket < 0 || destination_socket < 0) {
                Logger::info("Недопустимые сокеты в " + direction);
                break;
            }

            // Использование select для неблокирующего чтения
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(source_socket, &read_fds);

            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            int ready = select(source_socket + 1, &read_fds, nullptr, nullptr, &timeout);
            
            if (ready < 0) {
                if (errno == EINTR) {
                    continue; // Прерывание, продолжаем
                }
                Logger::error("Ошибка select в передаче данных (" + direction + "): " + 
                             strerror(errno));
                break;
            }
            
            if (ready == 0) {
                continue; // Таймаут
            }

            if (!FD_ISSET(source_socket, &read_fds)) {
                continue;
            }

            // Получение данных
            ssize_t bytes_received = recv(source_socket, buffer.data(), buffer.size(), 0);
            
            if (bytes_received <= 0) {
                if (bytes_received == 0) {
                    Logger::info("Соединение закрыто (" + direction + ")");
                } else if (errno != ECONNRESET && errno != EPIPE) {
                    Logger::error("Ошибка при получении данных (" + direction + "): " + 
                                 strerror(errno));
                }
                break;
            }

            // Отправка данных с проверкой частичной передачи
            ssize_t bytes_sent = 0;
            while (bytes_sent < bytes_received && running_.load()) {
                ssize_t sent = send(destination_socket, 
                                   buffer.data() + bytes_sent,
                                   bytes_received - bytes_sent, 0);
                
                if (sent <= 0) {
                    if (errno != ECONNRESET && errno != EPIPE && errno != ENOTCONN) {
                        Logger::error("Ошибка при отправке данных (" + direction + "): " + 
                                     strerror(errno));
                    }
                    running_.store(false);
                    return;
                }
                
                bytes_sent += sent;
            }
            
            total_bytes += bytes_received;
        }

    } catch (const std::exception& e) {
        Logger::error("Критическая ошибка в передаче данных (" + direction + "): " + 
                     std::string(e.what()));
    } catch (...) {
        Logger::error("Неизвестная ошибка в передаче данных (" + direction + ")");
    }
    
    Logger::info(direction + ": передано " + std::to_string(total_bytes) + " байт");
    Logger::info("Передача данных завершена (" + direction + ")");
    running_.store(false);
}

bool ProxyHandler::parse_http_connect(const std::string& connect_line, std::string& target_host, int& target_port) {
    try {
        // Парсим строку вида "CONNECT example.com:80 HTTP/1.1"
        std::istringstream iss(connect_line);
        std::string method, target, version;
        
        if (!(iss >> method >> target >> version)) {
            Logger::error("Неверный формат CONNECT запроса: " + connect_line);
            return false;
        }
        
        if (method != "CONNECT") {
            Logger::error("Ожидался метод CONNECT, получен: " + method);
            return false;
        }
        
        // Парсим target_host:target_port
        size_t colon_pos = target.find(':');
        if (colon_pos == std::string::npos) {
            Logger::error("Не найден порт в CONNECT запросе: " + target);
            return false;
        }
        
        target_host = target.substr(0, colon_pos);
        try {
            target_port = std::stoi(target.substr(colon_pos + 1));
        } catch (const std::exception& e) {
            Logger::error("Неверный порт в CONNECT запросе: " + target);
            return false;
        }
        
        is_http_connect_ = true;
        
        // Пропускаем остальные заголовки HTTP до пустой строки
        std::string line;
        char buffer[1024];
        int pos = 0;
        
        while (running_.load()) {
            pos = 0;
            while (pos < sizeof(buffer) - 1) {
                ssize_t received = recv(client_socket_, &buffer[pos], 1, 0);
                if (received <= 0) {
                    if (received == 0) {
                        Logger::info("Соединение закрыто при чтении заголовков HTTP");
                    } else {
                        Logger::error("Ошибка чтения заголовков: " + std::string(strerror(errno)));
                    }
                    return false;
                }
                
                if (buffer[pos] == '\n') {
                    if (pos > 0 && buffer[pos-1] == '\r') {
                        buffer[pos-1] = '\0';
                        line = std::string(buffer);
                        break;
                    } else {
                        buffer[pos] = '\0';
                        line = std::string(buffer);
                        break;
                    }
                }
                pos++;
            }
            
            // Пустая строка означает конец заголовков
            if (line.empty()) {
                break;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Исключение в parse_http_connect: " + std::string(e.what()));
        return false;
    } catch (...) {
        Logger::error("Неизвестное исключение в parse_http_connect");
        return false;
    }
}

bool ProxyHandler::parse_binary_protocol_from_buffer(char* buffer, int buffer_size, 
                                                    std::string& target_host, int& target_port) {
    // Это сложно, так как мы уже прочитали часть данных как строку
    // Для простоты вернем false - браузеры не используют наш бинарный протокол
    Logger::warning("Получен неизвестный протокол, ожидался HTTP CONNECT");
    return false;
}

bool ProxyHandler::parse_http_request(const std::string& request_line, std::string& target_host, int& target_port) {
    // Парсим строку вида "GET http://example.com/path HTTP/1.1"
    std::istringstream iss(request_line);
    std::string method, url, version;
    
    if (!(iss >> method >> url >> version)) {
        Logger::error("Неверный формат HTTP запроса: " + request_line);
        return false;
    }
    
    Logger::info("Получен HTTP " + method + " запрос к " + url);
    
    // Убираем протокол из URL
    std::string target_url = url;
    bool is_https = false;
    std::string path = "/";
    
    if (target_url.find("https://") == 0) {
        target_url = target_url.substr(8); // Убираем "https://"
        is_https = true;
        target_port = 443; // Порт по умолчанию для HTTPS
    } else if (target_url.find("http://") == 0) {
        target_url = target_url.substr(7); // Убираем "http://"
        target_port = 80; // Порт по умолчанию для HTTP
    } else {
        Logger::error("Неподдерживаемый протокол в URL: " + url);
        return false;
    }
    
    // Находим хост, порт и путь
    size_t path_pos = target_url.find('/');
    std::string host_port;
    if (path_pos != std::string::npos) {
        host_port = target_url.substr(0, path_pos);
        path = target_url.substr(path_pos);
    } else {
        host_port = target_url;
    }
    
    size_t colon_pos = host_port.find(':');
    if (colon_pos != std::string::npos) {
        target_host = host_port.substr(0, colon_pos);
        try {
            target_port = std::stoi(host_port.substr(colon_pos + 1));
        } catch (const std::exception& e) {
            Logger::error("Неверный порт в URL: " + host_port);
            return false;
        }
    } else {
        target_host = host_port;
        // target_port уже установлен выше (80 или 443)
    }
    
    // Формируем исходный HTTP запрос с относительным путем
    original_http_request_ = method + " " + path + " " + version + "\r\n";
    
    is_http_connect_ = false; // Это обычный HTTP запрос, не CONNECT
    
    // Читаем и сохраняем остальные заголовки HTTP
    std::string line;
    char buffer[1024];
    int pos = 0;
    
    while (true) {
        pos = 0;
        while (pos < sizeof(buffer) - 1) {
            if (recv(client_socket_, &buffer[pos], 1, 0) <= 0) {
                return false;
            }
            
            if (buffer[pos] == '\n') {
                if (pos > 0 && buffer[pos-1] == '\r') {
                    buffer[pos-1] = '\0';
                    line = std::string(buffer);
                    break;
                } else {
                    buffer[pos] = '\0';
                    line = std::string(buffer);
                    break;
                }
            }
            pos++;
        }
        
        // Пустая строка означает конец заголовков
        if (line.empty()) {
            original_http_request_ += "\r\n";
            break;
        } else {
            // Модифицируем заголовок Host, если нужно
            if (line.find("Host:") == 0) {
                original_http_request_ += "Host: " + target_host + "\r\n";
            } else {
                original_http_request_ += line + "\r\n";
            }
        }
    }
    
    return true;
}

void ProxyHandler::send_http_response(bool success) {
    std::string response;
    if (success) {
        response = "HTTP/1.1 200 Connection established\r\n\r\n";
    } else {
        response = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    }
    
    send(client_socket_, response.c_str(), response.length(), 0);
}

void ProxyHandler::forward_http_request() {
    if (original_http_request_.empty()) {
        Logger::error("Исходный HTTP запрос не сохранен");
        return;
    }
    
    Logger::info("Пересылка HTTP запроса через туннель");
    
    // Мутируем HTTP запрос и отправляем в туннель
    std::string request = original_http_request_;
    encrypt(reinterpret_cast<unsigned char*>(const_cast<char*>(request.c_str())), request.length());
    
    ssize_t sent = send(tunnel_socket_, request.c_str(), request.length(), 0);
    
    if (sent < 0) {
        Logger::error("Ошибка при отправке HTTP запроса в туннель: " + std::string(strerror(errno)));
    } else {
        Logger::info("HTTP запрос успешно переслан в туннель (" + std::to_string(sent) + " байт)");
    }
}

void ProxyHandler::send_mutated_target_info(const std::string& target_host, int target_port) {
    try {
        // Отправляем длину хоста (4 байта)
        uint32_t host_len = htonl(target_host.length()); // Приводим к сетевому порядку байтов
        char host_len_buf[4];
        memcpy(host_len_buf, &host_len, 4);
        encrypt(reinterpret_cast<unsigned char*>(host_len_buf), 4);
        
        if (send(tunnel_socket_, host_len_buf, 4, 0) != 4) {
            Logger::error("Ошибка отправки длины хоста");
            return;
        }
        
        // Отправляем хост
        std::vector<char> host_buf(target_host.begin(), target_host.end());
        encrypt(reinterpret_cast<unsigned char*>(host_buf.data()), host_buf.size());
        
        if (send(tunnel_socket_, host_buf.data(), host_buf.size(), 0) != static_cast<ssize_t>(host_buf.size())) {
            Logger::error("Ошибка отправки хоста");
            return;
        }
        
        // Отправляем порт (2 байта)
        uint16_t port = htons(target_port); // Приводим к сетевому порядку байтов
        char port_buf[2];
        memcpy(port_buf, &port, 2);
        encrypt(reinterpret_cast<unsigned char*>(port_buf), 2);
        
        if (send(tunnel_socket_, port_buf, 2, 0) != 2) {
            Logger::error("Ошибка отправки порта");
            return;
        }
        
        Logger::info("Отправлена мутированная информация о цели: " + target_host + 
                    ":" + std::to_string(target_port));
                    
    } catch (const std::exception& e) {
        Logger::error("Ошибка при отправке мутированной информации: " + std::string(e.what()));
    }
}

void ProxyHandler::encrypt(unsigned char* data, size_t size) {
    if (encryption_manager_.is_loaded()) {
        encryption_manager_.encrypt(data, size);
    } else {
        // Fallback к XOR, если не удалось загрузить алгоритм
        unsigned char key = config_.get_xor_key();
        for (size_t i = 0; i < size; ++i) {
            data[i] ^= key;
        }
    }
}