#!/usr/bin/env python3

import socket
import threading
import time
import sys

def log(msg):
    timestamp = time.strftime("%H:%M:%S")
    print(f"[{timestamp}] {msg}", flush=True)

def handle_client(client_socket, client_addr):
    try:
        log(f"=== Новое подключение от {client_addr} ===")
        
        # Читаем запрос
        data = client_socket.recv(8192)
        if not data:
            log(f"Пустой запрос от {client_addr}")
            return
            
        request = data.decode('utf-8', errors='ignore')
        log(f"Полный запрос от {client_addr}:")
        log("-" * 50)
        print(request)
        log("-" * 50)
        
        lines = request.split('\r\n')
        if not lines:
            log("Нет строк в запросе")
            return
            
        first_line = lines[0]
        log(f"Первая строка: {first_line}")
        
        if first_line.startswith('CONNECT'):
            # CONNECT запрос
            parts = first_line.split()
            if len(parts) < 2:
                log("Неправильный CONNECT запрос")
                return
                
            target = parts[1]
            host, port = target.split(':')
            port = int(port)
            
            log(f"CONNECT к {host}:{port}")
            
            try:
                # Подключаемся к цели
                target_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                target_socket.settimeout(10)
                log(f"Подключаемся к {host}:{port}...")
                target_socket.connect((host, port))
                log(f"Успешно подключились к {host}:{port}")
                
                # Отправляем 200 OK
                response = "HTTP/1.1 200 Connection established\r\n\r\n"
                client_socket.send(response.encode())
                log("Отправлен HTTP 200 Connection established")
                
                # Двусторонняя передача данных
                def forward(src, dst, direction):
                    try:
                        total = 0
                        while True:
                            data = src.recv(4096)
                            if not data:
                                log(f"Соединение {direction} закрыто (получено {total} байт)")
                                break
                            dst.send(data)
                            total += len(data)
                            if total % 1000 == 0:  # Логируем каждые 1000 байт
                                log(f"{direction}: передано {total} байт")
                    except Exception as e:
                        log(f"Ошибка в {direction}: {e}")
                    finally:
                        try:
                            src.close()
                            dst.close()
                        except:
                            pass
                
                # Запускаем потоки передачи
                t1 = threading.Thread(target=forward, args=(client_socket, target_socket, "client->target"))
                t2 = threading.Thread(target=forward, args=(target_socket, client_socket, "target->client"))
                
                t1.start()
                t2.start()
                
                t1.join(timeout=60)  # Максимум 60 секунд
                t2.join(timeout=60)
                
                log(f"Туннель к {host}:{port} завершен")
                
            except Exception as e:
                log(f"Ошибка подключения к {host}:{port}: {e}")
                error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n"
                client_socket.send(error_response.encode())
        
        else:
            log(f"Неподдерживаемый запрос: {first_line}")
            error_response = "HTTP/1.1 400 Bad Request\r\n\r\n"
            client_socket.send(error_response.encode())
            
    except Exception as e:
        log(f"Ошибка обработки {client_addr}: {e}")
    finally:
        try:
            client_socket.close()
        except:
            pass
        log(f"=== Соединение с {client_addr} закрыто ===")

def main():
    if len(sys.argv) > 1:
        port = int(sys.argv[1])
    else:
        port = 8085
    
    host = '127.0.0.1'
    
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server_socket.bind((host, port))
        server_socket.listen(10)
        
        log(f"Отладочный HTTP прокси запущен на {host}:{port}")
        log(f"Настройте в Telegram: HTTP прокси 127.0.0.1:{port}")
        log("Логируются ВСЕ запросы и данные")
        
        while True:
            client_socket, client_addr = server_socket.accept()
            client_thread = threading.Thread(target=handle_client, args=(client_socket, client_addr))
            client_thread.daemon = True
            client_thread.start()
            
    except KeyboardInterrupt:
        log("Остановка прокси сервера")
    except Exception as e:
        log(f"Ошибка сервера: {e}")
    finally:
        server_socket.close()

if __name__ == "__main__":
    main()