/**
 * @file thread_pool.h
 * @brief Thread pool for parallel task execution
 * @author JP Edge TTS Project
 * @date 2024
 *
 * @copyright MIT License
 */

#ifndef JP_EDGE_TTS_THREAD_POOL_H
#define JP_EDGE_TTS_THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>

namespace jp_edge_tts {

/**
 * @class ThreadPool
 * @brief A simple thread pool for parallel task execution
 *
 * @details Manages a pool of worker threads that can execute
 * tasks asynchronously, useful for parallel TTS synthesis.
 */
class ThreadPool {
public:
    /**
     * @brief Constructor
     * @param num_threads Number of worker threads (0 = hardware concurrency)
     */
    explicit ThreadPool(size_t num_threads = 0);

    /**
     * @brief Destructor - waits for all tasks to complete
     */
    ~ThreadPool();

    // Disable copy and move
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * @brief Enqueue a task for execution
     *
     * @tparam F Function type
     * @tparam Args Argument types
     * @param f Function to execute
     * @param args Function arguments
     * @return Future for the task result
     */
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

    /**
     * @brief Get number of worker threads
     * @return Thread count
     */
    size_t size() const { return workers.size(); }

    /**
     * @brief Get number of pending tasks
     * @return Task count
     */
    size_t pending() const;

    /**
     * @brief Check if thread pool is active
     * @return true if not stopped
     */
    bool is_active() const { return !stop; }

    /**
     * @brief Wait for all tasks to complete
     */
    void wait_all();

private:
    // Worker threads
    std::vector<std::thread> workers;

    // Task queue
    std::queue<std::function<void()>> tasks;

    // Synchronization
    mutable std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable finished;
    bool stop;
    size_t working;
};

// Template implementation
template<typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {

    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();

    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        if(stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        tasks.emplace([task](){ (*task)(); });
    }

    condition.notify_one();
    return res;
}

} // namespace jp_edge_tts

#endif // JP_EDGE_TTS_THREAD_POOL_H