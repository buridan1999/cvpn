#ifndef WINDOWS_THREADING_H
#define WINDOWS_THREADING_H

#ifdef _WIN32

/**
 * Windows threading compatibility layer для MinGW
 * Заменяет std::thread и std::mutex нативными Windows API
 */

#include <windows.h>
#include <process.h>
#include <functional>
#include <memory>

namespace std_compat {

// Простой mutex wrapper
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
};

// Lock guard helper
template<typename Mutex>
class lock_guard {
private:
    Mutex& mutex_;
    
public:
    explicit lock_guard(Mutex& m) : mutex_(m) {
        mutex_.lock();
    }
    
    ~lock_guard() {
        mutex_.unlock();
    }
    
    // Некопируемый
    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
};

// Thread wrapper
class thread {
private:
    HANDLE handle_;
    DWORD thread_id_;
    std::function<void()> func_;
    
    static unsigned __stdcall thread_proc(void* param) {
        auto* func = static_cast<std::function<void()>*>(param);
        try {
            (*func)();
        } catch (...) {
            // Ignore exceptions in thread
        }
        delete func;
        return 0;
    }
    
public:
    thread() : handle_(nullptr), thread_id_(0) {}
    
    template<typename Func, typename... Args>
    explicit thread(Func&& f, Args&&... args) : handle_(nullptr), thread_id_(0) {
        auto* func_ptr = new std::function<void()>(
            std::bind(std::forward<Func>(f), std::forward<Args>(args)...)
        );
        
        handle_ = reinterpret_cast<HANDLE>(
            _beginthreadex(nullptr, 0, thread_proc, func_ptr, 0, 
                          reinterpret_cast<unsigned*>(&thread_id_))
        );
        
        if (handle_ == nullptr) {
            delete func_ptr;
            throw std::runtime_error("Failed to create thread");
        }
    }
    
    ~thread() {
        if (handle_ && joinable()) {
            CloseHandle(handle_);
        }
    }
    
    // Move semantics
    thread(thread&& other) noexcept : handle_(other.handle_), thread_id_(other.thread_id_) {
        other.handle_ = nullptr;
        other.thread_id_ = 0;
    }
    
    thread& operator=(thread&& other) noexcept {
        if (this != &other) {
            if (handle_ && joinable()) {
                CloseHandle(handle_);
            }
            handle_ = other.handle_;
            thread_id_ = other.thread_id_;
            other.handle_ = nullptr;
            other.thread_id_ = 0;
        }
        return *this;
    }
    
    // Некопируемый
    thread(const thread&) = delete;
    thread& operator=(const thread&) = delete;
    
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
};

// make_unique helper
template<typename T, typename... Args>
std::unique_ptr<T> make_unique_compat(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

} // namespace std_compat

// Определяем псевдонимы для Windows
#define thread_type std_compat::thread
#define mutex_type std_compat::mutex
#define lock_guard_type std_compat::lock_guard
#define make_unique_thread std_compat::make_unique_compat

#else

// Для Linux используем стандартные типы
#include <thread>
#include <mutex>

#define thread_type std::thread
#define mutex_type std::mutex
#define lock_guard_type std::lock_guard
#define make_unique_thread std::make_unique

#endif // _WIN32

#endif // WINDOWS_THREADING_H