/**
 * Основные различия между Unix и Windows сетевыми API
 */

// Unix (Linux/macOS)          // Windows
#include <sys/socket.h>        #include <winsock2.h>
#include <netinet/in.h>        #include <ws2tcpip.h>
#include <arpa/inet.h>         

int socket(...)                SOCKET socket(...)
int close(fd)                  int closesocket(SOCKET)
ssize_t send(...)              int send(...) 
ssize_t recv(...)              int recv(...)
int setsockopt(int, int, int,  int setsockopt(SOCKET, int, int,
  const void*, socklen_t)        const char*, int)

inet_pton(...)                 WSAStringToAddress(...) или InetPton(...)
inet_ntop(...)                 WSAAddressToString(...) или InetNtop(...)

// Типы данных
socklen_t                      int
ssize_t                        int
int fd                         SOCKET fd

// Ошибки  
errno                          WSAGetLastError()
EAGAIN, EWOULDBLOCK           WSAEWOULDBLOCK
EINTR                          WSAEINTR

// Инициализация
// Ничего не нужно             WSAStartup() / WSACleanup()