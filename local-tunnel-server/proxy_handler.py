#!/usr/bin/env python3
"""
Proxy Handler - Обработчик прокси соединений
"""

import socket
import threading
import logging
import struct
import select
import time


class ProxyHandler:
    def __init__(self, client_socket, client_address, config, remove_callback):
        """Инициализация обработчика прокси"""
        self.client_socket = client_socket
        self.client_address = client_address
        self.config = config
        self.remove_callback = remove_callback
        self.logger = logging.getLogger(__name__)
        self.target_socket = None
        self.running = False
        
        # Настройки из конфигурации
        self.buffer_size = config['server']['buffer_size']
        self.timeout = config['server']['timeout']
        
        # Настройка таймаута для клиентского сокета
        self.client_socket.settimeout(self.timeout)
    
    def handle(self):
        """Основной метод обработки прокси соединения"""
        try:
            self.running = True
            
            # Получение информации о целевом сервере от клиента
            target_host, target_port = self.get_target_info()
            
            if not target_host or not target_port:
                self.logger.error(f"Не удалось получить информацию о целевом сервере от {self.client_address[0]}")
                return
            
            # Установка соединения с целевым сервером
            if not self.connect_to_target(target_host, target_port):
                self.logger.error(f"Не удалось подключиться к {target_host}:{target_port}")
                return
            
            self.logger.info(f"Установлен прокси туннель: {self.client_address[0]}:{self.client_address[1]} -> {target_host}:{target_port}")
            
            # Отправка подтверждения клиенту
            self.send_connection_response(True)
            
            # Запуск передачи данных
            self.start_data_transfer()
            
        except Exception as e:
            self.logger.error(f"Ошибка в обработчике прокси: {e}")
            self.send_connection_response(False)
        finally:
            self.close()
    
    def get_target_info(self):
        """Получение информации о целевом сервере от клиента"""
        try:
            # Простой протокол: первые 4 байта = длина хоста, следующие байты = хост, последние 2 байта = порт
            
            # Получение длины имени хоста
            host_len_data = self.recv_exact(4)
            if not host_len_data:
                return None, None
            
            host_len = struct.unpack('!I', host_len_data)[0]
            
            # Проверка разумной длины имени хоста
            if host_len > 255 or host_len == 0:
                return None, None
            
            # Получение имени хоста
            host_data = self.recv_exact(host_len)
            if not host_data:
                return None, None
            
            target_host = host_data.decode('utf-8')
            
            # Получение порта
            port_data = self.recv_exact(2)
            if not port_data:
                return None, None
            
            target_port = struct.unpack('!H', port_data)[0]
            
            return target_host, target_port
            
        except Exception as e:
            self.logger.error(f"Ошибка при получении информации о целевом сервере: {e}")
            return None, None
    
    def recv_exact(self, size):
        """Получение точно указанного количества байт"""
        data = b''
        while len(data) < size:
            try:
                chunk = self.client_socket.recv(size - len(data))
                if not chunk:
                    return None
                data += chunk
            except socket.timeout:
                self.logger.warning(f"Таймаут при получении данных от {self.client_address[0]}")
                return None
            except socket.error as e:
                self.logger.error(f"Ошибка сокета при получении данных: {e}")
                return None
        return data
    
    def connect_to_target(self, host, port):
        """Установка соединения с целевым сервером"""
        try:
            self.target_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.target_socket.settimeout(self.timeout)
            self.target_socket.connect((host, port))
            return True
            
        except Exception as e:
            self.logger.error(f"Не удалось подключиться к {host}:{port} - {e}")
            if self.target_socket:
                try:
                    self.target_socket.close()
                except:
                    pass
                self.target_socket = None
            return False
    
    def send_connection_response(self, success):
        """Отправка ответа о статусе соединения клиенту"""
        try:
            response = b'\x01' if success else b'\x00'
            self.client_socket.send(response)
        except Exception as e:
            self.logger.error(f"Ошибка при отправке ответа клиенту: {e}")
    
    def start_data_transfer(self):
        """Запуск передачи данных между клиентом и целевым сервером"""
        try:
            # Создание потоков для двунаправленной передачи данных
            client_to_target = threading.Thread(
                target=self.transfer_data,
                args=(self.client_socket, self.target_socket, "client->target"),
                daemon=True
            )
            
            target_to_client = threading.Thread(
                target=self.transfer_data,
                args=(self.target_socket, self.client_socket, "target->client"),
                daemon=True
            )
            
            # Запуск потоков
            client_to_target.start()
            target_to_client.start()
            
            # Ожидание завершения любого из потоков
            client_to_target.join()
            target_to_client.join()
            
        except Exception as e:
            self.logger.error(f"Ошибка при передаче данных: {e}")
    
    def transfer_data(self, source_socket, destination_socket, direction):
        """Передача данных между сокетами"""
        try:
            while self.running:
                # Использование select для неблокирующего чтения
                ready, _, _ = select.select([source_socket], [], [], 1.0)
                
                if not ready:
                    continue
                
                # Получение данных
                try:
                    data = source_socket.recv(self.buffer_size)
                    if not data:
                        break
                    
                    # Отправка данных
                    destination_socket.sendall(data)
                    
                    # Логирование передачи (только для отладки)
                    # self.logger.debug(f"Передано {len(data)} байт ({direction})")
                    
                except socket.timeout:
                    continue
                except socket.error as e:
                    self.logger.warning(f"Ошибка передачи данных ({direction}): {e}")
                    break
                    
        except Exception as e:
            self.logger.error(f"Критическая ошибка в передаче данных ({direction}): {e}")
        finally:
            self.logger.info(f"Передача данных завершена ({direction})")
            self.running = False
    
    def close(self):
        """Закрытие соединений"""
        self.running = False
        
        # Закрытие клиентского сокета
        if self.client_socket:
            try:
                self.client_socket.close()
            except:
                pass
        
        # Закрытие сокета целевого сервера
        if self.target_socket:
            try:
                self.target_socket.close()
            except:
                pass
        
        # Уведомление сервера об удалении клиента
        if self.remove_callback:
            self.remove_callback(self)