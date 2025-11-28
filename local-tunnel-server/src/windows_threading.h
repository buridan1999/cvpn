#ifndef WINDOWS_THREADING_H
#define WINDOWS_THREADING_H

/**
 * Совместимая библиотека threading для MinGW
 * Заменяет std::thread и std::mutex нативными Windows API
 */

#ifdef _WIN32

#include <windows.h>
#include <process.h>
#include <memory>
#include <functional>
#include <stdexcept>

namespace threading {

// Windows mutex wrapper
class mutex {
private:
    CRITICAL_SECTION cs_;

public:
    mutex() {
        InitializeCriticalSection(&cs_);
    }
    
    ~mutex() {
        DeleteCriticalSection(&cs_);
    }
    
    void lock() {
        EnterCriticalSection(&cs_);
    }
    
    void unlock() {
        LeaveCriticalSection(&cs_);
    }
    
    bool try_lock() {
        return TryEnterCriticalSection(&cs_) != 0;
    }
    
    // Disable copy
    mutex(const mutex&) = delete;
    mutex& operator=(const mutex&) = delete;
};

// Windows lock_guard
template<class M>
class lock_guard {
private:
    M& mutex_;

public:
    explicit lock_guard(M& m) : mutex_(m) {
        mutex_.lock();
    }
    
    ~lock_guard() {
        mutex_.unlock();
    }
    
    // Disable copy
    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
};

// Windows thread wrapper
class thread {
private:
    HANDLE handle_{nullptr};
    DWORD thread_id_{0};
    
    struct ThreadData {
        std::function<void()> function;
    };
    
    static unsigned __stdcall thread_proc(void* data) {
        std::unique_ptr<ThreadData> td(static_cast<ThreadData*>(data));
        try {
            td->function();
        } catch (...) {
            // Игнорируем исключения в thread'е
        }
        return 0;
    }

public:
    thread() = default;
    
    template<class F, class... Args>
    explicit thread(F&& f, Args&&... args) {
        auto bound = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto data = new ThreadData{bound};
        
        handle_ = reinterpret_cast<HANDLE>(_beginthreadex(
            nullptr,
            0,
            thread_proc,
            data,
            0,
            reinterpret_cast<unsigned*>(&thread_id_)
        ));
        
        if (!handle_) {
            delete data;
            throw std::runtime_error("Failed to create thread");
        }
    }
    
    ~thread() {
        if (handle_ && joinable()) {
            detach(); // Просто отсоединяем поток вместо terminate
        }
    }
    
    thread(const thread&) = delete;
    thread& operator=(const thread&) = delete;
    
    thread(thread&& other) noexcept 
        : handle_(other.handle_), thread_id_(other.thread_id_) {
        other.handle_ = nullptr;
        other.thread_id_ = 0;
    }
    
    thread& operator=(thread&& other) noexcept {
        if (this != &other) {
            if (handle_ && joinable()) {
                detach(); // Отсоединяем старый поток вместо terminate
            }
            handle_ = other.handle_;
            thread_id_ = other.thread_id_;
            other.handle_ = nullptr;
            other.thread_id_ = 0;
        }
        return *this;
    }
    
    void join() {
        if (handle_) {
            WaitForSingleObject(handle_, INFINITE);
            CloseHandle(handle_);
            handle_ = nullptr;
            thread_id_ = 0;
        }
    }
    
    bool joinable() const {
        return handle_ != nullptr;
    }
    
    void detach() {
        if (handle_) {
            CloseHandle(handle_);
            handle_ = nullptr;
            thread_id_ = 0;
        }
    }
    
    DWORD get_id() const {
        return thread_id_;
    }
};

// make_unique helper для thread'ов
template<typename... Args>
std::unique_ptr<thread> make_unique_thread(Args&&... args) {
    return std::unique_ptr<thread>(new thread(std::forward<Args>(args)...));
}

} // namespace threading

// Поддержка std::this_thread для Windows
namespace std {
    namespace this_thread {
        inline DWORD get_id() {
            return GetCurrentThreadId();
        }
    }
}

// Типы для использования в коде
using thread_type = threading::thread;
using mutex_type = threading::mutex;

template<typename T>
using lock_guard_type = threading::lock_guard<T>;

#else

// Для Linux используем стандартные типы
#include <thread>
#include <mutex>

using thread_type = std::thread;
using mutex_type = std::mutex;

template<typename T>
using lock_guard_type = std::lock_guard<T>;

namespace threading {
    template<typename... Args>
    std::unique_ptr<std::thread> make_unique_thread(Args&&... args) {
        return std::make_unique<std::thread>(std::forward<Args>(args)...);
    }
}

#endif // _WIN32

#endif // WINDOWS_THREADING_H