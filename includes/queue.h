#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

#include "exc.h"

namespace flow {

template <typename T, std::enable_if_t<
    std::conjunction<
        std::is_same<std::remove_cvref_t<T>, T>,
        std::is_copy_constructible<T>
    >::value, bool> = true>
class tqueue {
    int m_capacity;
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_waiter;
    bool m_shutdown;
public:
    tqueue(int capacity = -1) : m_queue(), m_mutex(), m_waiter(), m_shutdown(false) {}
    tqueue(const tqueue& other) : m_queue(other.m_queue), m_mutex(), m_waiter(), m_shutdown(other.m_shutdown) {}

    void put(const T& element, bool throw_on_fail = false) {
        std::unique_lock<std::mutex> lk(m_mutex);
        if (m_shutdown) throw flow::shutdown();
        if (m_capacity > 0 && m_queue.size() == m_capacity) {
            if (throw_on_fail) throw full();
            else m_waiter.wait(lk, [this](){ return m_queue.size() < m_capacity; });
        }
        m_queue.push(element);
        m_waiter.notify_one();
    }

    T get(bool throw_on_fail = false) {
        std::unique_lock<std::mutex> lk(m_mutex);
        if (m_queue.empty()) {
            if (m_shutdown) throw flow::shutdown();
            else if (throw_on_fail) throw empty();
            else m_waiter.wait(lk, [this](){ return !m_queue.empty(); });
        }
        T value = m_queue.front();
        m_queue.pop();
        m_waiter.notify_one();
        return value;
    }

    void shutdown() {
        std::unique_lock<std::mutex> lk(m_mutex);
        m_shutdown = true;
    }

    inline void put_nowait(const T& element) {
        put(element, true);
    }

    inline T get_nowait() {
        return get(true);
    }

    size_t size() const {
        // no benefit to locking here
        return m_queue.size();
    }
};

}
