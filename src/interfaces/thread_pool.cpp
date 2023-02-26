#include <coffee/interfaces/thread_pool.hpp>

namespace coffee {

    ThreadPool::ThreadPool(uint32_t amountOfThreads) {
        const uint32_t hardwareAmountOfThreads = std::thread::hardware_concurrency();

        if (hardwareAmountOfThreads > 0 && amountOfThreads > hardwareAmountOfThreads) {
            amountOfThreads = hardwareAmountOfThreads;
        }

        createThreads(std::max(1U, amountOfThreads));
    }

    ThreadPool::~ThreadPool() {
        waitForTasks();
        destroyThreads();
    }

    void ThreadPool::push(std::function<void()>&& func) {
        {
            std::unique_lock<std::mutex> lock_ { tasksMutex_ };
            tasks_.push(std::move(func));
        }

        tasksTotal_++;
        cvTaskAvailable_.notify_one();
    }

    void ThreadPool::waitForTasks() {
        waiting_ = true;

        std::unique_lock<std::mutex> lock_ { tasksMutex_ };
        cvTaskDone_.wait(lock_, [&]() { return tasksTotal_ == 0; });

        waiting_ = false;
    }

    void ThreadPool::createThreads(uint32_t amountOfThreads) {
        threads_.reserve(amountOfThreads);

        for (uint32_t i = 0; i < amountOfThreads; ++i) {
            threads_.emplace_back(&ThreadPool::worker, this);
        }
    }

    void ThreadPool::destroyThreads() {
        running_ = false;
        cvTaskAvailable_.notify_all();

        for (size_t i = 0; i < threads_.size(); i++) {
            threads_[i].join();
        }
    }

    void ThreadPool::worker() {
        while (running_) {
            std::unique_lock<std::mutex> lock_ { tasksMutex_ };
            cvTaskAvailable_.wait(lock_, [&]() { return !tasks_.empty() || !running_; });

            if (!running_) {
                return;
            }

            std::function<void()> task = std::move(tasks_.front());
            tasks_.pop();

            lock_.unlock();
            try { task(); } catch (...) {}
            lock_.lock();

            tasksTotal_--;

            if (waiting_) {
                cvTaskDone_.notify_one();
            }
        }
    }

}