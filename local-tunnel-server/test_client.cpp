#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Использование: " << argv[0] << " <vpn_server_ip> <vpn_server_port> <target_host:target_port>" << std::endl;
        std::cout << "Пример: " << argv[0] << " 127.0.0.1 8080 google.com:80" << std::endl;
        return 1;
    }
    
    std::string vpn_host = argv[1];
    int vpn_port = std::atoi(argv[2]);
    std::string target_address = argv[3];
    
    // Парсинг целевого адреса
    size_t colon_pos = target_address.find(':');
    if (colon_pos == std::string::npos) {
        std::cerr << "Некорректный формат целевого адреса" << std::endl;
        return 1;
    }
    
    std::string target_host = target_address.substr(0, colon_pos);
    int target_port = std::atoi(target_address.substr(colon_pos + 1).c_str());
    
    // Создание сокета
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Не удалось создать сокет" << std::endl;
        return 1;
    }
    
    // Подключение к VPN серверу
    sockaddr_in vpn_addr{};
    vpn_addr.sin_family = AF_INET;
    vpn_addr.sin_port = htons(vpn_port);
    inet_pton(AF_INET, vpn_host.c_str(), &vpn_addr.sin_addr);
    
    if (connect(sock, reinterpret_cast<sockaddr*>(&vpn_addr), sizeof(vpn_addr)) < 0) {
        std::cerr << "Не удалось подключиться к VPN серверу" << std::endl;
        close(sock);
        return 1;
    }
    
    std::cout << "Подключен к VPN серверу " << vpn_host << ":" << vpn_port << std::endl;
    
    // Отправка информации о целевом сервере
    uint32_t host_len = htonl(target_host.length());
    uint16_t port_net = htons(target_port);
    
    // Отправка длины хоста
    if (send(sock, &host_len, sizeof(host_len), 0) < 0) {
        std::cerr << "Ошибка при отправке длины хоста" << std::endl;
        close(sock);
        return 1;
    }
    
    // Отправка хоста
    if (send(sock, target_host.c_str(), target_host.length(), 0) < 0) {
        std::cerr << "Ошибка при отправке хоста" << std::endl;
        close(sock);
        return 1;
    }
    
    // Отправка порта
    if (send(sock, &port_net, sizeof(port_net), 0) < 0) {
        std::cerr << "Ошибка при отправке порта" << std::endl;
        close(sock);
        return 1;
    }
    
    // Получение ответа от сервера
    uint8_t response;
    if (recv(sock, &response, sizeof(response), 0) <= 0) {
        std::cerr << "Ошибка при получении ответа от сервера" << std::endl;
        close(sock);
        return 1;
    }
    
    if (response != 1) {
        std::cerr << "Сервер не смог установить соединение с " << target_host << ":" << target_port << std::endl;
        close(sock);
        return 1;
    }
    
    std::cout << "Туннель установлен к " << target_host << ":" << target_port << std::endl;
    std::cout << "Теперь можно отправлять данные через прокси" << std::endl;
    
    // Простой тест - отправка HTTP запроса (если это веб-сервер)
    if (target_port == 80 || target_port == 8080) {
        std::string http_request = "GET / HTTP/1.1\r\nHost: " + target_host + "\r\nConnection: close\r\n\r\n";
        if (send(sock, http_request.c_str(), http_request.length(), 0) > 0) {
            std::cout << "HTTP запрос отправлен" << std::endl;
            
            // Получение ответа
            char buffer[4096];
            int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                std::cout << "Получен ответ:" << std::endl;
                std::cout << buffer << std::endl;
            }
        }
    }
    
    close(sock);
    return 0;
}