#pragma once

#include <any>
#include <concepts> // IWYU pragma: keep
#include <map>
#include <type_traits>

namespace flow {

template <typename T> requires std::copy_constructible<T>
class Source;

template <typename T> requires std::copy_constructible<T>
class Sink;

class Flow {
    uint32_t m_idCounter;
    std::map<uint32_t, std::any> m_sourcesAndSinks;
public:
    template <typename T> requires std::copy_constructible<T>
    friend class Source;

    template <typename T> requires std::copy_constructible<T>
    friend class Sink;

    Flow() : m_idCounter(0), m_sourcesAndSinks() {}

    // TODO: Need some sort of non-generic container to store generic source & sinks??

    template <typename T, typename U, typename... Args, std::enable_if_t<
        std::conjunction<
            std::is_base_of<Source<U>, T>,
            std::is_constructible<Source<U>, Flow&, Args...>,
            std::is_copy_constructible<Source<U>>
        >::value, bool> = true
    >
    Source<U>& source(Args&&... args) {
        uint32_t id = m_idCounter++;
        Source<U> src(*this, std::forward(args)...);
        auto result = m_sourcesAndSinks.emplace(std::make_pair(id, src));
        result.first->second.m_flowId = id;
        return result.first->second;
    }

    template <typename T, typename U, typename... Args, std::enable_if_t<
        std::conjunction<
            std::is_base_of<Sink<U>, T>,
            std::is_constructible<Sink<U>, Flow&, Args...>,
            std::is_copy_constructible<Sink<U>>
        >::value, bool> = true
    >
    Sink<U>& sink(Args&&... args) {
        uint32_t id = m_idCounter++;
        Sink<U> sink(*this, std::forward(args)...);
        auto result = m_sourcesAndSinks.emplace(std::make_pair(id, sink));
        result.first->second.m_flowId = id;
        return result.first->second;
    }
};

}
