#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

/**
 * Заголовок совместимости для кросс-платформенной сборки
 * Обеспечивает единый интерфейс для Windows и Unix/Linux
 */

#ifdef _WIN32
    // Windows headers
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <io.h>
    
    // Thread and mutex for Windows (MinGW compatibility)
    #include <thread>
    #include <mutex>
    #include <chrono>
    
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
    typedef long long ssize_t;
    #define MSG_WAITALL 0x8
    #define MSG_PEEK 0x2
    #define SHUT_RDWR SD_BOTH
    #define close(fd) closesocket(fd)
    
    // POSIX compatibility for Windows
    #ifndef EINTR
    #define EINTR WSAEINTR
    #endif
    
    // Network functions
    inline int inet_pton_compat(int af, const char* src, void* dst) {
        struct sockaddr_storage ss;
        int size = sizeof(ss);
        char src_copy[INET6_ADDRSTRLEN+1];
        
        if (strlen(src) >= INET6_ADDRSTRLEN+1) return 0;
        strcpy(src_copy, src);
        
        if (WSAStringToAddressA(src_copy, af, NULL, (struct sockaddr*)&ss, &size) == 0) {
            if (af == AF_INET) {
                *(struct in_addr*)dst = ((struct sockaddr_in*)&ss)->sin_addr;
                return 1;
            }
        }
        return 0;
    }
    #define inet_pton inet_pton_compat
    
    inline const char* inet_ntop_compat(int af, const void* src, char* dst, socklen_t size) {
        struct sockaddr_storage ss;
        DWORD len = size;
        
        memset(&ss, 0, sizeof(ss));
        ss.ss_family = af;
        
        if (af == AF_INET) {
            ((struct sockaddr_in*)&ss)->sin_addr = *(struct in_addr*)src;
        } else {
            return NULL;
        }
        
        if (WSAAddressToStringA((struct sockaddr*)&ss, sizeof(ss), NULL, dst, &len) == 0) {
            return dst;
        }
        return NULL;
    }
    #define inet_ntop inet_ntop_compat

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
    #include <chrono>
    
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
inline int inet_pton_windows(int af, const char* src, void* dst) {
#ifdef _WIN32
    struct sockaddr_storage ss;
    int size = sizeof(ss);
    char src_copy[INET6_ADDRSTRLEN+1];
    
    if (strlen(src) >= INET6_ADDRSTRLEN+1) return 0;
    strcpy(src_copy, src);
    
    if (WSAStringToAddressA(src_copy, af, NULL, (struct sockaddr*)&ss, &size) == 0) {
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
#define inet_pton_compat inet_pton_windows
#else
#define inet_pton_compat inet_pton
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

/**
 * Совместимая функция setsockopt для таймаутов
 */
inline int set_socket_timeout(SOCKET sock, int timeout_sec) {
#ifdef _WIN32
    DWORD timeout = timeout_sec * 1000;
    int ret1 = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    int ret2 = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    return (ret1 == 0 && ret2 == 0) ? 0 : -1;
#else
    struct timeval timeout;
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;
    int ret1 = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    int ret2 = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    return (ret1 == 0 && ret2 == 0) ? 0 : -1;
#endif
}

/**
 * Совместимая функция для SO_REUSEADDR
 */
inline int set_socket_reuse(SOCKET sock) {
#ifdef _WIN32
    BOOL opt = TRUE;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    int opt = 1;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
}

#endif // PLATFORM_COMPAT_H