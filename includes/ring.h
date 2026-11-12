#pragma once

#include <algorithm>
#include <concepts> // IWYU pragma: keep
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "exc.h"

namespace flow {

template <typename T> requires std::copy_constructible<T>
class ringbuffer;

template <typename T>
class prqueue {
    friend class ringbuffer<T>;

    ringbuffer<T>* m_parent;
    size_t m_pointer;
    // if we hit the back of the queue through calls to get(), we have
    // "touched the back" and cannot get any more until another call
    // to put().
    mutable bool m_touchedBack;
    std::condition_variable_any m_waiter;

    prqueue(ringbuffer<T>* parent) : m_parent(parent), m_touchedBack(false), m_waiter() {
        std::unique_lock lk(parent->m_mutex);
        m_pointer = parent->m_front;
        parent->m_consumers++;
        parent->m_queueWaiters.push_back(&m_waiter);
    }

    prqueue(const prqueue<T>& other) = delete;
    prqueue(prqueue<T>&& other) : m_parent(other.m_parent), m_touchedBack(other.m_touchedBack), m_waiter() {
        other.m_parent = nullptr;
    }
public:
    virtual ~prqueue() {
        if (!m_parent) return;
        std::unique_lock lk(m_parent->m_mutex);
        m_parent->m_consumers--;
        // NOTE: possible edge case - full ringbuffer, prqueue which has consumed nothing and is destroyed.
        // expected behavior is a normal, successful destruction.
        while (m_pointer != m_parent->m_back) {
            m_parent->m_buffer[m_pointer].second++;
            m_pointer = (m_pointer + 1) % m_parent->m_capacity;
        }
        while (m_parent->check());
        std::vector<std::condition_variable_any*>& v = m_parent->m_queueWaiters;
        std::vector<std::condition_variable_any*>::iterator waiterEntry = std::find(v.begin(), v.end(), &m_waiter);
        if (waiterEntry != m_parent->m_queueWaiters.end()) m_parent->m_queueWaiters.erase(waiterEntry);
    }

    T get(bool throw_on_fail = false) {
        std::shared_lock lk(m_parent->m_mutex);
        if (m_pointer == m_parent->m_front && m_touchedBack) m_touchedBack = false;
        if ((m_pointer == m_parent->m_back && m_touchedBack) || m_parent->m_length == 0) {
            if (throw_on_fail) {
                throw empty();
            } else {
                // whenever m_pointer is no longer equal to m_parent->m_back, we will be able
                // to get another item
                m_parent->m_waiting++;
                m_waiter.wait(lk, [this](){ return m_pointer != m_parent->m_back && m_parent->m_length > 0; });
            }
        }
        m_parent->m_buffer[m_pointer].second++;
        const T& result = m_parent->m_buffer[m_pointer].first;
        m_pointer = (m_pointer + 1) % m_parent->m_capacity;
        // if pointer reaches back AFTER an increment, we know that we have
        // "touched the back". 
        if (m_pointer == m_parent->m_back) m_touchedBack = true;
        m_parent->m_waiter.notify_one();
        m_parent->check();
        return result;
    }

    inline T get_nowait() {
        return get(true);
    }

    size_t available() const {
        std::shared_lock lk(m_parent->m_mutex);
        if (m_parent->m_length == 0) return 0;
        if (m_pointer == m_parent->m_front && m_touchedBack) m_touchedBack = false;
        if (m_pointer == m_parent->m_back) {
            if (m_touchedBack) return 0;
            // only other case is if m_front == m_back in which
            // case we have the entire capacity to go through
            return m_parent->m_capacity;
        } else {
            if (m_parent->m_back > m_pointer) return m_parent->m_back - m_pointer;
            else return (m_parent->m_back + m_parent->m_capacity) - m_pointer;
        }
    }
};

template <typename T> requires std::copy_constructible<T>
class ringbuffer {
    using entry = std::pair<T, std::atomic_uint8_t>;

    friend class prqueue<T>;
    std::unique_ptr<entry[]> m_buffer;
    size_t m_capacity;
    size_t m_length;
    size_t m_consumers;
    size_t m_front;
    size_t m_back;
    std::shared_mutex m_mutex;
    // used to prevent multiple functions which modify
    // m_front, m_back, and m_length from running at
    // the same time
    std::mutex m_structureMutex;
    std::condition_variable_any m_waiter;
    std::vector<std::condition_variable_any*> m_queueWaiters;
    std::atomic_uint8_t m_waiting;

    bool check() {
        std::lock_guard<std::mutex> guard(m_structureMutex);
        if (m_length > 0) {
            if (m_buffer[m_front].second == m_consumers) {
                m_front++;
                m_length--;
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
public:
    ringbuffer(size_t capacity) : m_buffer(new entry[capacity]), m_capacity(capacity), m_length(0),
        m_consumers(0), m_front(0), m_back(0), m_mutex(), m_structureMutex(), m_waiter(), m_queueWaiters(), m_waiting(0) {}
    ringbuffer(const ringbuffer<T>& other) : m_buffer(new entry[other.m_capacity]), m_capacity(other.m_capacity), m_length(other.m_length),
        m_consumers(0), m_front(other.m_front), m_back(other.m_back), m_mutex(), m_structureMutex(), m_waiter(), m_queueWaiters(), m_waiting(0) {
        memcpy(m_buffer.get(), other.m_buffer.get(), sizeof(entry) * m_capacity);
    }

    void put(const T& element, bool throw_on_fail = false) {
        std::unique_lock<std::mutex> guard(m_structureMutex);
        std::unique_lock<std::shared_mutex> lk(m_mutex);
        if (m_length == m_capacity) {
            if (throw_on_fail) {
                throw full();
            } else {
                lk.unlock();
                m_waiter.wait(guard, [this]() { return m_length < m_capacity; });
                lk.lock();
            }
        }
        m_buffer[m_back].first = element;
        m_buffer[m_back].second = 0;
        m_back = (m_back + 1) % m_capacity;
        m_length++;

        if (m_waiting > 0) {
            for (std::condition_variable_any* waiter : m_queueWaiters) waiter->notify_one();
            m_waiting = 0;
        }
    }

    inline void put_nowait(const T& element) {
        put(element, true);
    }

    prqueue<T> spawnqueue() {
        return prqueue<T>(this);
    }

    size_t capacity() const noexcept {
        return m_capacity;
    }
    size_t length() const noexcept {
        return m_length;
    }
    size_t consumers() const noexcept {
        return m_consumers;
    }
};

}
