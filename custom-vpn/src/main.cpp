#include "vpn_server.h"
#include "logger.h"
#include "config.h"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

int main(int argc, char* argv[]) {
    std::cout << "=== Custom VPN Server ===" << std::endl;
    std::cout << "TCP Proxy Server для Linux (C++)" << std::endl;
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
        
        std::cout << "Создание сервера..." << std::endl;
        
        // Создание и запуск сервера
        VPNServer server(config_file);
        
        std::cout << "Запуск сервера..." << std::endl;
        
        if (!server.start()) {
            Logger::error("Не удалось запустить VPN сервер");
            return 1;
        }

        std::cout << "Сервер запущен. Нажмите Ctrl+C для остановки." << std::endl;
        
        // Главный цикл
        while (server.is_running()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Опционально: вывод статистики каждые 30 секунд
            static int counter = 0;
            if (++counter >= 30) {
                counter = 0;
                auto status = server.get_status();
                Logger::info("Статус сервера: активных клиентов = " + 
                           std::to_string(status.active_clients));
            }
        }
        
        Logger::info("Завершение работы главного потока");
        
    } catch (const std::exception& e) {
        std::cerr << "Критическая ошибка: " << e.what() << std::endl;
        Logger::error("Критическая ошибка: " + std::string(e.what()));
        return 1;
    }

    return 0;
}