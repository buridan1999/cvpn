#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

/**
 * Заголовок совместимости для кросс-платформенной сборки
 * Обеспечивает единый интерфейс для Windows и Unix/Linux
 */

#ifdef _WIN32
    // Windows headers
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <io.h>
    
    // Windows socket timeout compatibility
    inline int setsockopt_compat(SOCKET s, int level, int optname, const void* optval, int optlen) {
        if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
            DWORD timeout = *(const int*)optval * 1000; // convert to milliseconds
            return setsockopt(s, level, optname, (const char*)&timeout, sizeof(timeout));
        }
        return setsockopt(s, level, optname, (const char*)optval, optlen);
    }
    #define setsockopt setsockopt_compat
    
    // Windows socket compatibility
    typedef int socklen_t;
    #define MSG_WAITALL 0x8
    #define SHUT_RDWR SD_BOTH
    #define close(fd) closesocket(fd)
    
    // POSIX compatibility for Windows
    #ifndef EINTR
    #define EINTR WSAEINTR
    #endif
    
    // Thread compatibility
    #include <thread>
    #include <mutex>
    
#else
    // Unix/Linux headers  
    #include <sys/socket.h>
    #include <sys/select.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    #include <thread>
    #include <mutex>
    
    // Unix socket compatibility
    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    
#endif

/**
 * Инициализация сокетов (для Windows WSA)
 */
inline bool init_sockets() {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
    return true;
#endif
}

/**
 * Очистка сокетов (для Windows WSA)
 */
inline void cleanup_sockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

/**
 * Получение последней ошибки сокета
 */
inline int get_last_socket_error() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

/**
 * Проверка на временную ошибку (EINTR, EAGAIN)
 */
inline bool is_temporary_error(int error) {
#ifdef _WIN32
    return error == WSAEINTR || error == WSAEWOULDBLOCK;
#else
    return error == EINTR || error == EAGAIN || error == EWOULDBLOCK;
#endif
}

/**
 * inet_pton для Windows совместимости
 */
inline int inet_pton_compat(int af, const char* src, void* dst) {
#ifdef _WIN32
    struct sockaddr_storage ss;
    int size = sizeof(ss);
    char src_copy[INET6_ADDRSTRLEN+1];
    strncpy(src_copy, src, INET6_ADDRSTRLEN+1);
    if (WSAStringToAddress(src_copy, af, NULL, (struct sockaddr*)&ss, &size) == 0) {
        if (af == AF_INET) {
            *(struct in_addr*)dst = ((struct sockaddr_in*)&ss)->sin_addr;
            return 1;
        }
    }
    return 0;
#else
    return inet_pton(af, src, dst);
#endif
}

#ifdef _WIN32
#define inet_pton inet_pton_compat
#endif

/**
 * Кросс-платформенный sleep в миллисекундах
 */
inline void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

#endif // PLATFORM_COMPAT_H