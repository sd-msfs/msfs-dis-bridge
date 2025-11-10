/**
 * @file ThreadSafeQueue.hpp
 * @brief Thread-safe queue for multi-threaded communication
 */

#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

/**
 * @brief A thread-safe wrapper around std::queue for multi-threaded environments
 *
 * This template class provides a thread-safe queue implementation that can be
 * safely used across multiple threads. It supports both blocking and non-blocking
 * operations, making it suitable for producer-consumer patterns in the MSFS DIS
 * Bridge system.
 *
 * @tparam T The type of elements stored in the queue
 *
 * @details The implementation uses:
 * - std::mutex for thread synchronization
 * - std::condition_variable for efficient blocking operations
 * - Move semantics for optimal performance
 * - RAII principles for automatic resource management
 *
 * @note This class is non-copyable and non-movable to prevent accidental
 * sharing of the underlying synchronization primitives.
 */
template <typename T>
class ThreadSafeQueue {
public:
    /**
     * @brief Default constructor
     *
     * Creates an empty thread-safe queue ready for use.
     */
    ThreadSafeQueue() = default;

    /**
     * @brief Default destructor
     *
     * Automatically cleans up all resources. Any threads waiting on
     * dequeue() operations may experience undefined behavior if the
     * queue is destroyed while they are waiting.
     *
     * @warning Ensure all consumer threads have finished before
     * destroying the queue to avoid undefined behavior.
     */
    ~ThreadSafeQueue() = default;

    /**
     * @brief Copy constructor (deleted)
     *
     * Copying is disabled to prevent issues with shared synchronization
     * primitives and to enforce clear ownership semantics.
     */
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;

    /**
     * @brief Copy assignment operator (deleted)
     *
     * Assignment is disabled to prevent issues with shared synchronization
     * primitives and to enforce clear ownership semantics.
     */
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    /**
     * @brief Add an item to the back of the queue
     *
     * Safely adds an item to the queue and notifies one waiting consumer
     * thread (if any). The operation is atomic and thread-safe.
     *
     * @param item The item to add to the queue (moved into the queue)
     *
     * @note Uses move semantics for efficiency - the passed item will be
     * moved from, so don't rely on its state after this call
     *
     * @see dequeue(), try_dequeue()
     */
    void enqueue(T item) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(std::move(item));
        }
        cv_.notify_one();
    }

    /**
     * @brief Remove and return an item from the front of the queue (blocking)
     *
     * Removes and returns the first item in the queue. If the queue is empty,
     * this method will block the calling thread until an item becomes available.
     *
     * @return T The item removed from the front of the queue
     *
     * @note This method blocks indefinitely if the queue remains empty.
     * Use try_dequeue() for non-blocking behavior.
     *
     * @warning If the queue is destroyed while a thread is waiting in this
     * method, undefined behavior may occur.
     *
     * @see enqueue(), try_dequeue()
     */
    T dequeue() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]{ return !queue_.empty(); });
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    /**
     * @brief Try to remove an item from the queue without blocking
     *
     * Attempts to remove and return an item from the front of the queue.
     * If the queue is empty, returns std::nullopt immediately without blocking.
     *
     * @return std::optional<T> The item if available, or std::nullopt if queue is empty
     *
     * @note This method never blocks, making it suitable for polling scenarios
     * or when you need to check multiple queues.
     *
     * @see enqueue(), dequeue()
     */
    std::optional<T> try_dequeue() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    /**
     * @brief Check if the queue is empty
     *
     * Returns true if the queue contains no elements. This is a thread-safe
     * snapshot of the queue state at the time of the call.
     *
     * @return true if the queue is empty
     * @return false if the queue contains one or more elements
     *
     * @note The result may become outdated immediately after the call
     * returns due to concurrent operations from other threads.
     *
     * @see size()
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    /**
     * @brief Get the current number of elements in the queue
     *
     * Returns the number of elements currently in the queue. This is a
     * thread-safe snapshot of the queue size at the time of the call.
     *
     * @return size_t The number of elements in the queue
     *
     * @note The result may become outdated immediately after the call
     * returns due to concurrent operations from other threads.
     *
     * @see empty()
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }

private:
    /**
     * @brief Mutex for protecting queue operations
     *
     * Provides mutual exclusion for all queue operations to ensure
     * thread safety. Marked mutable to allow const methods to acquire locks.
     */
    mutable std::mutex mtx_;

    /**
     * @brief Underlying queue storage
     *
     * The actual std::queue that stores the elements. Protected by mtx_
     * to ensure thread-safe access.
     */
    std::queue<T> queue_;

    /**
     * @brief Condition variable for blocking operations
     *
     * Used to efficiently implement blocking dequeue operations.
     * Consumers wait on this condition variable when the queue is empty,
     * and producers notify it when adding items.
     */
    std::condition_variable cv_;
};
