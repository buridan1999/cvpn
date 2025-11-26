#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>

class SafeTest {
private:
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> test_thread_;

public:
    ~SafeTest() {
        try {
            stop();
        } catch (...) {
            // Ignore all exceptions in destructor
        }
    }
    
    void start() {
        running_.store(true);
        test_thread_ = std::make_unique<std::thread>([this]() {
            try {
                while (running_.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    std::cout << "Working..." << std::endl;
                }
            } catch (...) {
                std::cout << "Exception caught in thread" << std::endl;
            }
            std::cout << "Thread finished" << std::endl;
        });
    }
    
    void stop() {
        if (!running_.load()) return;
        
        running_.store(false);
        
        try {
            if (test_thread_ && test_thread_->joinable()) {
                test_thread_->join();
            }
        } catch (...) {
            // Ignore
        }
    }
};

int main() {
    try {
        auto test = std::make_shared<SafeTest>();
        test->start();
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        test->stop();
        std::cout << "Test completed successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "Unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}