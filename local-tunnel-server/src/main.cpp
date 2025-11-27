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
        
        std::unique_ptr<VPNServer> proxy_server;
        std::unique_ptr<TunnelServer> tunnel_server;
        
        // Создание серверов в зависимости от режима
        if (mode == Config::ServerMode::BOTH || mode == Config::ServerMode::TUNNEL_ONLY) {
            tunnel_server = std::make_unique<TunnelServer>(config);
        }
        
        if (mode == Config::ServerMode::BOTH || mode == Config::ServerMode::PROXY_ONLY) {
            proxy_server = std::make_unique<VPNServer>(config);
        }
        
        // Запуск туннель-сервера
        if (tunnel_server) {
            std::cout << "Запуск туннель-сервера..." << std::endl;
            if (!tunnel_server->start()) {
                Logger::error("Не удалось запустить Tunnel сервер");
                return 1;
            }
        }
        
        // Запуск прокси-сервера
        if (proxy_server) {
            std::cout << "Запуск прокси-сервера..." << std::endl;
            if (!proxy_server->start()) {
                Logger::error("Не удалось запустить Proxy сервер");
                if (tunnel_server) tunnel_server->stop();
                return 1;
            }
        }

        std::cout << "Серверы запущены:" << std::endl;
        if (proxy_server) {
            std::cout << "- Proxy Server: " << config.get_server_host() << ":" << config.get_server_port() << std::endl;
        }
        if (tunnel_server) {
            std::cout << "- Tunnel Server: " << config.get_tunnel_host() << ":" << config.get_tunnel_port() << std::endl;
        }
        std::cout << "- XOR Key: " << static_cast<int>(config.get_xor_key()) << std::endl;
        std::cout << "Нажмите Ctrl+C для остановки." << std::endl;
        
        // Главный цикл
        bool running = true;
        while (running) {
            if (proxy_server && !proxy_server->is_running()) {
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
                    if (proxy_server) {
                        auto proxy_status = proxy_server->get_status();
                        status_msg += "Proxy клиентов = " + std::to_string(proxy_status.active_clients);
                    }
                    if (tunnel_server) {
                        auto tunnel_status = tunnel_server->get_status();
                        if (proxy_server) status_msg += ", ";
                        status_msg += "Tunnel соединений = " + std::to_string(tunnel_status.active_tunnels);
                    }
                    Logger::info(status_msg);
                }
            }
        }
        
        std::cout << "Остановка серверов..." << std::endl;
        if (proxy_server) {
            proxy_server->stop();
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