/**
 * @file thread_pool.cpp
 * @brief Implementation of thread pool for parallel execution
 * @author D Everett Hinton
 * @date 2025
 *
 * @copyright MIT License
 */

#include "jp_edge_tts/utils/thread_pool.h"
#include <algorithm>

namespace jp_edge_tts {

ThreadPool::ThreadPool(size_t num_threads) : stop(false), working(0) {
    // Use hardware concurrency if not specified
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;  // Fallback
    }

    // Create worker threads
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this] {
            for (;;) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);

                    // Wait for task or stop signal
                    this->condition.wait(lock, [this] {
                        return this->stop || !this->tasks.empty();
                    });

                    if (this->stop && this->tasks.empty()) {
                        return;
                    }

                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                    working++;
                }

                // Execute task
                task();

                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    working--;
                    if (working == 0 && tasks.empty()) {
                        finished.notify_all();
                    }
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }

    condition.notify_all();

    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

size_t ThreadPool::pending() const {
    std::unique_lock<std::mutex> lock(queue_mutex);
    return tasks.size();
}

void ThreadPool::wait_all() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    finished.wait(lock, [this] {
        return tasks.empty() && working == 0;
    });
}

} // namespace jp_edge_tts