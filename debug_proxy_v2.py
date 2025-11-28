#!/usr/bin/env python3
import socket
import threading
import sys
from datetime import datetime

def log(message):
    timestamp = datetime.now().strftime("[%H:%M:%S]")
    print(f"{timestamp} {message}")

def forward_data(src, dst, direction):
    """Переправляет данные между двумя сокетами"""
    try:
        total = 0
        while True:
            data = src.recv(8192)
            if not data:
                log(f"Поток {direction} завершён (передано {total} байт)")
                break
            dst.send(data)
            total += len(data)
    except Exception as e:
        log(f"Ошибка в потоке {direction}: {e}")
    finally:
        try:
            src.shutdown(socket.SHUT_RD)
        except:
            pass

def handle_http_request(client_socket, request_lines):
    """Обрабатывает обычные HTTP запросы (GET, POST и т.д.)"""
    first_line = request_lines[0]
    parts = first_line.split()
    
    if len(parts) < 2:
        log("Неверный формат HTTP запроса")
        return False
    
    method = parts[0]
    url = parts[1]
    
    log(f"HTTP {method} запрос: {url}")
    
    # Парсим URL
    if url.startswith('http://'):
        url_without_scheme = url[7:]  # убираем 'http://'
        if '/' in url_without_scheme:
            host_port, path = url_without_scheme.split('/', 1)
            path = '/' + path
        else:
            host_port = url_without_scheme
            path = '/'
        
        if ':' in host_port:
            host, port = host_port.split(':', 1)
            port = int(port)
        else:
            host = host_port
            port = 80
        
        log(f"Перенаправляем {method} {path} к {host}:{port}")
        
        try:
            # Подключаемся к целевому серверу
            target_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            target_socket.settimeout(10)
            target_socket.connect((host, port))
            
            # Модифицируем запрос для отправки на целевой сервер
            modified_first_line = f"{method} {path} HTTP/1.1"
            
            # Исправляем Host заголовок
            modified_headers = []
            for line in request_lines[1:]:
                if line.lower().startswith('host:'):
                    modified_headers.append(f"Host: {host}")
                else:
                    modified_headers.append(line)
            
            modified_request = modified_first_line + '\r\n' + '\r\n'.join(modified_headers)
            
            log(f"Отправляем на сервер: {modified_first_line}")
            log(f"Host заголовок: {host}")
            
            # Отправляем весь запрос
            target_socket.send(modified_request.encode())
            
            # Получаем ответ от сервера и отправляем клиенту
            response_received = 0
            first_response = True
            while True:
                try:
                    target_socket.settimeout(5)  # Уменьшаем timeout для быстрой диагностики
                    data = target_socket.recv(8192)
                    if not data:
                        break
                    client_socket.send(data)
                    response_received += len(data)
                    
                    if first_response:  # Логируем первую часть ответа
                        response_text = data.decode('utf-8', errors='ignore')
                        log(f"ОТВЕТ СЕРВЕРА: {response_text[:300]}")
                        first_response = False
                except socket.timeout:
                    log("Timeout при получении ответа от сервера")
                    break
                except Exception as e:
                    log(f"Ошибка при передаче данных: {e}")
                    break
            
            log(f"Передано {response_received} байт от сервера к клиенту")
            target_socket.close()
            return True
            
        except Exception as e:
            log(f"Ошибка HTTP запроса к {host}:{port}: {e}")
            error_response = "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 11\r\n\r\nProxy Error"
            client_socket.send(error_response.encode())
            return False
    else:
        log(f"Неподдерживаемый URL: {url}")
        error_response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\n\r\nInvalid URL"
        client_socket.send(error_response.encode())
        return False

def handle_connect_request(client_socket, request_lines):
    """Обрабатывает CONNECT запросы для HTTPS"""
    first_line = request_lines[0]
    parts = first_line.split()
    
    if len(parts) < 2:
        log("Неверный формат CONNECT запроса")
        return False
    
    target = parts[1]
    if ':' in target:
        host, port = target.split(':', 1)
        port = int(port)
    else:
        host = target
        port = 443
    
    log(f"CONNECT к {host}:{port}")
    
    try:
        # Подключаемся к целевому серверу
        target_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        target_socket.settimeout(10)
        target_socket.connect((host, port))
        
        # Отправляем ответ "200 Connection established"
        response = "HTTP/1.1 200 Connection established\r\nProxy-agent: debug-proxy/2.0\r\n\r\n"
        client_socket.send(response.encode())
        log(f"Отправлен ответ 200, начинаем туннель")
        
        # Создаем два потока для двунаправленной передачи данных
        client_to_server = threading.Thread(
            target=forward_data, 
            args=(client_socket, target_socket, "клиент->сервер")
        )
        server_to_client = threading.Thread(
            target=forward_data, 
            args=(target_socket, client_socket, "сервер->клиент")
        )
        
        client_to_server.start()
        server_to_client.start()
        
        # Ждем завершения потоков
        client_to_server.join()
        server_to_client.join()
        
        target_socket.close()
        return True
        
    except Exception as e:
        log(f"Ошибка CONNECT к {host}:{port}: {e}")
        error_response = "HTTP/1.1 502 Bad Gateway\r\n\r\n"
        client_socket.send(error_response.encode())
        return False

def handle_client(client_socket, client_addr):
    log(f"=== Новое подключение от {client_addr} ===")
    
    try:
        # Получаем запрос
        data = client_socket.recv(8192)
        if not data:
            log("Нет данных от клиента")
            return
        
        request = data.decode('utf-8', errors='ignore')
        log(f"Полный запрос от {client_addr}:")
        log("-" * 50)
        print(request)
        log("-" * 50)
        
        request_lines = request.split('\r\n')
        if not request_lines:
            log("Нет строк в запросе")
            return
        
        first_line = request_lines[0]
        log(f"Первая строка: {first_line}")
        
        if first_line.startswith('CONNECT '):
            # HTTP CONNECT запрос (для HTTPS)
            success = handle_connect_request(client_socket, request_lines)
        elif first_line.startswith(('GET ', 'POST ', 'PUT ', 'DELETE ', 'HEAD ', 'OPTIONS ')):
            # Обычный HTTP запрос (как делает Telegram)
            success = handle_http_request(client_socket, request_lines)
        else:
            log(f"Неподдерживаемый тип запроса: {first_line}")
            error_response = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 18\r\n\r\nMethod Not Allowed"
            client_socket.send(error_response.encode())
            success = False
        
        if success:
            log(f"Запрос успешно обработан для {client_addr}")
        else:
            log(f"Ошибка обработки запроса для {client_addr}")
            
    except Exception as e:
        log(f"Ошибка обработки клиента {client_addr}: {e}")
    finally:
        try:
            client_socket.close()
        except:
            pass
        log(f"=== Соединение с {client_addr} закрыто ===")

def main():
    if len(sys.argv) != 2:
        print("Использование: python3 debug_proxy_v2.py <порт>")
        sys.exit(1)
    
    port = int(sys.argv[1])
    
    # Создаем сервер
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('127.0.0.1', port))
    server.listen(10)
    
    log(f"Расширенный HTTP прокси запущен на 127.0.0.1:{port}")
    log("Поддерживает: HTTP CONNECT (HTTPS) + обычные HTTP запросы (HTTP)")
    log("Настройте в Telegram: HTTP прокси 127.0.0.1:" + str(port))
    log("Логируются ВСЕ запросы и ответы")
    
    try:
        while True:
            client_socket, client_addr = server.accept()
            client_thread = threading.Thread(
                target=handle_client, 
                args=(client_socket, client_addr)
            )
            client_thread.daemon = True
            client_thread.start()
    except KeyboardInterrupt:
        log("Остановка прокси сервера")
    finally:
        server.close()

if __name__ == "__main__":
    main()