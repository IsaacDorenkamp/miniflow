#pragma once

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <shared_mutex>
#include <utility>
#include <vector>

namespace flow {

class full : public std::exception {};
class empty : public std::exception {};
class shutdown : public std::exception {};

template <typename T>
class ringbuffer;

template <typename T>
class prqueue {
    friend class ringbuffer<T>;

    ringbuffer<T>* m_parent;
    size_t m_pointer;
    // if we hit the back of the queue through calls to get(), we have
    // "touched the back" and cannot get any more until another call
    // to put().
    bool m_touchedBack;
    std::condition_variable_any m_waiter;

    prqueue(ringbuffer<T>* parent) : m_parent(parent), m_touchedBack(false) {
        std::unique_lock lk(parent->m_mutex);
        m_pointer = parent->m_front;
        parent->m_consumers++;
        parent->m_queueWaiters.push_back(&m_waiter);
    }
public:
    virtual ~prqueue() {
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
};

template <typename T>
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
    std::condition_variable_any m_waiter;
    std::vector<std::condition_variable_any*> m_queueWaiters;
    std::atomic_uint8_t m_waiting;

    bool check() {
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
        m_consumers(0), m_front(0), m_back(0), m_mutex(), m_waiter(), m_queueWaiters(), m_waiting(0) {}

    void put(const T& element, bool throw_on_fail = false) {
        std::unique_lock<std::shared_mutex> lk(m_mutex);
        if (m_length == m_capacity) {
            if (throw_on_fail) {
                throw full();
            } else {
                m_waiter.wait(lk, [this]() { return m_length < m_capacity; });
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
