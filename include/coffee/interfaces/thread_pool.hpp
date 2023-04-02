#ifndef COFFEE_INTERFACES_THREAD_POOL
#define COFFEE_INTERFACES_THREAD_POOL

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>

namespace coffee {

    class ThreadPool {
    public:
        ThreadPool(uint32_t amountOfThreads = std::thread::hardware_concurrency());
        ~ThreadPool();

        template <typename F, typename... Args, typename = std::enable_if_t<std::is_invocable_v<F, Args...>>>
        void push(F&& callable, Args&&... args) {
            this->pushTask(std::bind(std::forward<F>(callable), std::forward<Args>(args)...));
        }

        void waitForTasks();

    private:
        void createThreads(uint32_t amountOfThreads);
        void pushTask(std::function<void()>&& func);
        void destroyThreads();

        void worker();

        std::atomic_bool running_ { true };
        std::atomic_bool waiting_ { false };

        std::condition_variable cvTaskAvailable_ {};
        std::condition_variable cvTaskDone_ {};

        std::queue<std::function<void()>> tasks_ {};
        std::atomic<size_t> tasksTotal_ { 0 };
        mutable std::mutex tasksMutex_ {};

        std::vector<std::thread> threads_ {};
    };

} // namespace coffee

#endif