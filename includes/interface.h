#pragma once

#include <cstddef>
#include <type_traits>

namespace flow {

template <typename T, typename U>
concept Puttable = std::is_invocable<typename T::put, const std::remove_cvref_t<U>&>::value;

template <typename T, typename U> requires Puttable<T, U>
class putcounter {
    T* m_proxied;
    size_t m_count;
public:
    using ParamType = const std::remove_cvref_t<U>&;
    putcounter(T& proxied) : m_proxied(&proxied), m_count(0) {}
    void put(ParamType element) {
        m_proxied->put(element);
        m_count++;
    }
    void reset() noexcept {
        m_count = 0;
    }
    size_t count() const noexcept {
        return m_count;
    }
};

template <typename T, typename U>
concept Gettable = std::is_invocable_r<U, typename T::get>::value;

template <typename T, typename U> requires Gettable<T, U>
class getcounter {
    T* m_proxied;
    size_t m_count;
public:
    getcounter(T& proxied) : m_proxied(&proxied), m_count(0) {}
    U get() {
        U value = m_proxied->get();
        m_count++;
        return value;
    }
    void reset() noexcept {
        m_count = 0;
    }
    size_t count() const noexcept {
        return m_count;
    }
};

}
