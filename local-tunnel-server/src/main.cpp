#include "vpn_server.h"
#include "tunnel_server.h"
#include "logger.h"
#include "config.h"
#include "platform_compat.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>
#include <memory>

int main(int argc, char* argv[]) {
    // Инициализация сокетов для Windows
    if (!init_sockets()) {
        std::cerr << "Ошибка инициализации сокетов!" << std::endl;
        return 1;
    }
    
    std::cout << "=== Local Tunnel Server ===" << std::endl;
    std::cout << "Двухсокетный Proxy/Tunnel Server (C++)" << std::endl;
    std::cout << "------------------------------" << std::endl;

    try {
        // Определение файла конфигурации
        std::string config_file = "config.json";
        if (argc > 1) {
            config_file = argv[1];
        }

        // Создание конфигурации
        Config config(config_file);
        
        std::cout << "Конфигурация загружена, инициализация логгера..." << std::endl;
        
        // Инициализация системы логирования
        Logger::init(config.get_log_level(), config.get_log_file(), config.get_log_format());
        
        std::cout << "Создание серверов..." << std::endl;
        
        // Определяем режим работы сервера
        Config::ServerMode mode = config.get_server_mode();
        
        std::unique_ptr<VPNServer> vpn_server;     // Удалённый VPN сервер (порт 8080)
        std::unique_ptr<TunnelServer> tunnel_server; // Локальный туннель (порт 8081)
        
        // Создание серверов в зависимости от режима
        if (mode == Config::ServerMode::BOTH || mode == Config::ServerMode::TUNNEL_ONLY) {
            vpn_server = std::make_unique<VPNServer>(config);    // Удалённый VPN сервер
        }
        
        if (mode == Config::ServerMode::BOTH || mode == Config::ServerMode::PROXY_ONLY) {
            tunnel_server = std::make_unique<TunnelServer>(config); // Локальный туннель (браузер подключается сюда)
        }
        
        // Запуск VPN сервера (удалённый - принимает зашифрованные соединения)
        if (vpn_server) {
            std::cout << "Запуск VPN сервера (удалённая часть)..." << std::endl;
            if (!vpn_server->start()) {
                Logger::error("Не удалось запустить VPN сервер");
                return 1;
            }
        }
        
        // Запуск Tunnel сервера (локальный - браузер подключается сюда)
        if (tunnel_server) {
            std::cout << "Запуск Tunnel сервера (локальный туннель)..." << std::endl;
            if (!tunnel_server->start()) {
                Logger::error("Не удалось запустить Tunnel сервер");
                if (vpn_server) vpn_server->stop();
                return 1;
            }
        }

        std::cout << "Серверы запущены:" << std::endl;
        if (tunnel_server) {
            std::cout << "- Tunnel Server (браузер подключается сюда): " << config.get_tunnel_host() << ":" << config.get_tunnel_port() << std::endl;
        }
        if (vpn_server) {
            std::cout << "- VPN Server (удалённый сервер): " << config.get_server_host() << ":" << config.get_server_port() << std::endl;
        }
        std::cout << "- XOR Key: " << static_cast<int>(config.get_xor_key()) << std::endl;
        std::cout << "Нажмите Ctrl+C для остановки." << std::endl;
        
        // Главный цикл
        bool running = true;
        while (running) {
            if (vpn_server && !vpn_server->is_running()) {
                running = false;
            }
            if (tunnel_server && !tunnel_server->is_running()) {
                running = false;
            }
            
            if (running) {
                sleep_ms(1000);  // Используем кросс-платформенную версию
                
                // Вывод статистики каждые 30 секунд
                static int counter = 0;
                if (++counter >= 30) {
                    counter = 0;
                    std::string status_msg = "Статус: ";
                    if (vpn_server) {
                        auto vpn_status = vpn_server->get_status();
                        status_msg += "VPN клиентов = " + std::to_string(vpn_status.active_clients);
                    }
                    if (tunnel_server) {
                        auto tunnel_status = tunnel_server->get_status();
                        if (vpn_server) status_msg += ", ";
                        status_msg += "Tunnel соединений = " + std::to_string(tunnel_status.active_tunnels);
                    }
                    Logger::info(status_msg);
                }
            }
        }
        
        std::cout << "Остановка серверов..." << std::endl;
        if (vpn_server) {
            vpn_server->stop();
        }
        if (tunnel_server) {
            tunnel_server->stop();
        }
        
        Logger::info("Завершение работы");
        
        // Очистка сокетов для Windows
        cleanup_sockets();
        
    } catch (const std::exception& e) {
        std::cerr << "Критическая ошибка: " << e.what() << std::endl;
        Logger::error("Критическая ошибка: " + std::string(e.what()));
        cleanup_sockets();
        return 1;
    }

    return 0;
}