/**
 * @file ThreadSafeQueue.hpp
 * @brief Thread-safe queue implementation with blocking and non-blocking operations
 */

#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

/**
 * @class ThreadSafeQueue
 * @brief A thread-safe FIFO queue with blocking and non-blocking access methods
 *
 * This template class provides a producer-consumer queue that is safe for
 * concurrent access from multiple threads. It supports both blocking dequeue
 * operations (waiting for items to become available) and non-blocking try_dequeue
 * operations (returning immediately if empty).
 *
 * Key features:
 * - Thread-safe enqueue and dequeue operations
 * - Blocking dequeue with condition variable notification
 * - Non-blocking try_dequeue for polling scenarios
 * - Move semantics for efficient element transfer
 * - Thread-safe size and empty queries
 *
 * @tparam T The type of elements stored in the queue. Must be move-constructible.
 *
 * @par Thread Safety
 * All public methods are fully thread-safe and can be called concurrently from
 * multiple threads. Internal synchronization uses a mutex and condition variable.
 *
 * @par Performance Considerations
 * - enqueue() uses move semantics to avoid unnecessary copies
 * - dequeue() blocks the calling thread until an item is available
 * - try_dequeue() never blocks and is suitable for polling loops
 * - All operations have O(1) time complexity
 *
 * @par Example Usage
 * @code
 * ThreadSafeQueue<std::vector<uint8_t>> pduQueue;
 *
 * // Producer thread:
 * std::vector<uint8_t> packet = createPacket();
 * pduQueue.enqueue(std::move(packet));
 *
 * // Consumer thread (blocking):
 * auto data = pduQueue.dequeue();  // Waits if queue is empty
 *
 * // Consumer thread (non-blocking):
 * if (auto data = pduQueue.try_dequeue()) {
 *     processPacket(*data);
 * }
 * @endcode
 *
 * @see UDPMulticaster
 */
template <typename T>
class ThreadSafeQueue {
public:
    /**
     * @brief Default constructor
     */
    ThreadSafeQueue() = default;

    /**
     * @brief Destructor
     *
     * @warning If threads are still blocked in dequeue(), they will remain
     *          blocked until an item is enqueued or the application terminates.
     *          Ensure all consumer threads have exited before destroying the queue.
     */
    ~ThreadSafeQueue() = default;

    // Disable copy/move to prevent unintended sharing
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;              ///< No copy constructor
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;   ///< No copy assignment

    /**
     * @brief Adds an item to the queue (thread-safe)
     *
     * Pushes the provided item onto the back of the queue and notifies one
     * waiting thread (if any are blocked in dequeue()). Uses move semantics
     * to efficiently transfer ownership.
     *
     * @param item Item to add to the queue (will be moved)
     *
     * @note This method never blocks
     * @note Notifies exactly one waiting thread via condition variable
     *
     * @par Thread Safety
     * Multiple threads can safely call enqueue() concurrently. The operation
     * is protected by an internal mutex.
     *
     * @par Performance
     * O(1) amortized time complexity. The item is moved, not copied.
     */
    void enqueue(T item) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(std::move(item));
        }
        cv_.notify_one();
    }

    /**
     * @brief Removes and returns an item from the queue (blocking)
     *
     * Removes the front item from the queue and returns it. If the queue is
     * empty, this method blocks the calling thread until an item becomes
     * available (via enqueue() call from another thread).
     *
     * @return The front item from the queue (moved)
     *
     * @warning This method blocks indefinitely if the queue remains empty.
     *          Consider using try_dequeue() if blocking is not acceptable.
     *
     * @par Thread Safety
     * Multiple threads can safely call dequeue() concurrently. When an item
     * is enqueued, exactly one waiting thread will be awakened.
     *
     * @par Performance
     * O(1) time complexity when item is available. Blocking wait when empty.
     *
     * @par Example
     * @code
     * ThreadSafeQueue<int> queue;
     *
     * // Consumer thread:
     * int value = queue.dequeue();  // Blocks until producer enqueues
     * @endcode
     */
    T dequeue() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]{ return !queue_.empty(); });
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    /**
     * @brief Attempts to remove an item from the queue (non-blocking)
     *
     * Removes and returns the front item if the queue is non-empty. If empty,
     * returns std::nullopt immediately without blocking.
     *
     * @return std::optional<T> containing the front item if available,
     *         or std::nullopt if queue is empty
     *
     * @note This method never blocks, making it suitable for polling scenarios
     *
     * @par Thread Safety
     * Multiple threads can safely call try_dequeue() concurrently.
     *
     * @par Example
     * @code
     * ThreadSafeQueue<std::string> queue;
     *
     * if (auto item = queue.try_dequeue()) {
     *     std::cout << "Got: " << *item << "\n";
     * } else {
     *     std::cout << "Queue is empty\n";
     * }
     * @endcode
     */
    std::optional<T> try_dequeue() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    /**
     * @brief Checks if the queue is empty (thread-safe)
     *
     * @return true if the queue contains no elements, false otherwise
     *
     * @note The result may become stale immediately after this call returns
     *       if other threads are concurrently modifying the queue
     *
     * @par Thread Safety
     * This method is thread-safe but provides a snapshot in time. The queue
     * state may change immediately after this call returns.
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

    /**
     * @brief Returns the number of elements in the queue (thread-safe)
     *
     * @return Number of elements currently in the queue
     *
     * @note The result may become stale immediately after this call returns
     *       if other threads are concurrently modifying the queue
     *
     * @par Thread Safety
     * This method is thread-safe but provides a snapshot in time.
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }

private:
    mutable std::mutex mtx_;           ///< Mutex protecting queue access
    std::queue<T> queue_;              ///< Underlying FIFO queue
    std::condition_variable cv_;       ///< Condition variable for blocking dequeue
};
