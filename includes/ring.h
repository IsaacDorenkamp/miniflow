#pragma once

#include <cstddef>
#include <memory>

namespace flow {

template <typename T>
class ringvec {
public:
    ringvec(size_t capacity = 1024) : m_buffer(new T[capacity]), m_capacity(capacity), m_length(0), m_front(0), m_back(0) {}
    virtual ~ringvec() = default;

    class iterator {
    public:
        explicit iterator(ringvec* parent, size_t index) : m_parent(parent), m_index(index) {}
        iterator& operator++() { m_index++; return *this; }
        iterator operator++(int) { iterator ret = *this; (*this)++; return ret; }
        bool operator==(const iterator& other) const { return m_parent == other.m_parent && m_index == other.m_index; }
        bool operator!=(const iterator& other) const { return !(*this == other); }
        const T& operator*() const { return (*m_parent)[m_index]; }
    private:
        ringvec* m_parent;
        size_t m_index;
    };

    iterator begin() {
        return iterator(this, 0);
    }
    iterator end() {
        return iterator(this, m_length);
    }
    iterator cbegin() const {
        return iterator(this, 0);
    }
    iterator cend() const {
        return iterator(this, m_length);
    }

    bool push_back(const T& element) {
        size_t backNext = (m_back + 1) % (m_capacity + 1);
        if (backNext == m_front) return false;
        m_buffer[m_back] = element;
        m_back = backNext;
        m_length++;
        return true;
    }

    bool pop_front() {
        if (m_front == m_back) return false;
        m_front = (m_front + 1) % m_capacity;
        m_length--;
        return true;
    }

    size_t length() const { return m_length; }
    size_t capacity() const { return m_capacity; }

    void resize(size_t capacity) {
        if (capacity == m_capacity) return;
        using std::make_unique, std::unique_ptr;
        unique_ptr<T[]> old = std::move(m_buffer);
        m_capacity = capacity;
        m_buffer = make_unique<T[]>(m_capacity);
        size_t count;
        size_t front;
        for (count = 0, front = m_front; front != m_back && count < m_capacity; front++, count++) m_buffer[count] = old[front];
        m_front = 0;
        m_back = 0;
        m_length = std::min(m_length, m_capacity);
    }

    const T& operator[](size_t index) {
        return m_buffer[(m_front + index) % m_capacity];
    }
private:
    std::unique_ptr<T[]> m_buffer;
    size_t m_capacity;
    size_t m_length;

    size_t m_front;
    size_t m_back;
};

}
