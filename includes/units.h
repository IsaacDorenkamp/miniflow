#pragma once

#include <concepts>  // IWYU pragma: keep
#include <exception>
#include <optional>
#include <queue>
#include <typeindex>

#include "ring.h"

namespace flow {

typedef struct _as_abstract {} as_abstract_t;

extern as_abstract_t as_abstract;

class BadConnector : public std::exception {
    const char* m_typeFor;
    std::type_index m_type;
    std::string m_message;
public:
    BadConnector(const char* typeFor, std::type_index type) : m_typeFor(typeFor), m_type(type), m_message(typeFor) {
        m_message.append(" expected connector for type ");
        m_message.append(type.name());
    }
    const char* what() const noexcept {
        return m_message.c_str();
    }
};

template <typename T>
class Source {
public:
    class Unavailable : public std::exception {};

    virtual T produce() = 0;
};

template <std::copy_constructible T>
class Connector {
    std::optional<T> m_stuck; 
    ringbuffer<T> m_buffer;
public:
    Connector(size_t capacity) : m_stuck(), m_buffer(capacity) {}

    bool put(const T& value) {
        try {
            m_buffer.put_nowait(value);
            return true;
        } catch (const full&) {
            m_stuck = value;
            return false;
        }
    }

    prqueue<T> observe() {
        return m_buffer.spawnqueue();
    }

    bool blocked() const {
        return m_stuck.has_value();
    }

    bool unblock() {
        try {
            m_buffer.put_nowait(*m_stuck);
            m_stuck.reset();
            return true;
        } catch (const full&) {
            return false;
        }
    }
};

template <typename T>
class Sink {
public:
    virtual void consume(const T& element) = 0;
};

class NodeInterface {
public:
    enum Result {
        SUCCESS,
        SOURCE_UNAVAILABLE,
        SOURCE_EXCEPTION,
        SOURCE_UNBLOCKED,  // when process() succeeds in resolving a blockage
        SOURCE_BLOCKED, // when the connector becomes blocked
        SINK_EXCEPTION
    };

    virtual Result process() = 0;
};

template <typename T, typename U, std::enable_if_t<std::is_base_of<Source<T>, U>::value, bool> = true>
class SourceInterface : public NodeInterface {
    U* m_source;
    std::vector<Connector<T>*> m_sinks;
    std::queue<Connector<T>*> m_stuck;

    std::optional<std::exception> m_exc;
public:
    SourceInterface(U& source) : m_source(&source), m_sinks(), m_stuck(), m_exc() {}
    Result process() final {
        // first, try to resolve any blockages
        size_t nStuck = m_stuck.size();
        if (nStuck) {
            Connector<T>* conn;
            bool unstuck;
            // try to unblock each connector, pushing it to the back of the queue again
            // if it fails
            for (size_t i = 0; i < nStuck; i++) {
                conn = m_stuck.front();
                m_stuck.pop();
                unstuck = conn->unblock();
                if (!unstuck) m_stuck.push(conn);
            }
            if (m_stuck.size() > 0) return SOURCE_BLOCKED;
            else return SOURCE_UNBLOCKED;
        }

        try {
            T element = m_source->produce();
            int failed = 0;
            for (Connector<T>* connector : m_sinks) {
                bool placed = connector->put(element);
                if (!placed) {
                    failed++;
                    m_stuck.push(connector);
                }
            }
            return (failed > 0) ? SUCCESS : SOURCE_BLOCKED;
        } catch (const Source<T>::Unavailable&) {
            return SOURCE_UNAVAILABLE;
        } catch (const std::exception& exc) {
            m_exc = exc;
            return SOURCE_EXCEPTION;
        }
    }

    const std::optional<std::exception>& exception() const {
        return m_exc;
    }
};

template <std::copy_constructible T, typename U, std::enable_if_t<std::is_base_of<Sink<T>, U>::value, bool> = true>
class SinkInterface : public NodeInterface {
    U* m_sink;
    prqueue<T> m_queue;
    std::optional<std::exception> m_exc;
public:
    SinkInterface(U& sink, Connector<T>& connector) : m_sink(&sink), m_queue(connector.observe()) {}
    Result process() final {
        T element = m_queue.get();
        try {
            m_sink->consume(element);
        } catch (const std::exception& exc) {
            m_exc = exc;
            return SINK_EXCEPTION;
        }
    }

    const std::optional<std::exception>& exception() const {
        return m_exc;
    }
};

template <std::copy_constructible In, std::copy_constructible Out, typename U,
         std::enable_if_t<
            std::conjunction<
                std::is_base_of<Sink<In>, U>,
                std::is_base_of<Source<Out>, U>
            >::value, bool> = true>
class SourceSinkInterface : public NodeInterface {
    U* m_sourceSink;
    prqueue<In> m_source;
    Connector<Out>* m_sink;
    std::optional<std::exception> m_exc;
public:
    SourceSinkInterface(U& sourceSink, Connector<In>& from, Connector<Out>& to) : m_sourceSink(&sourceSink), m_source(from.observe()), m_sink(&m_sink), m_exc() {}
    Result process() final {
        In element = m_source.get();
        try {
            m_sourceSink->consume(element);
        } catch (const std::exception& exc) {
            m_exc = exc;
            return SINK_EXCEPTION;
        }

        try {
            Out result = m_sourceSink->produce();
            bool placed = m_sink->put(result);
            return placed ? SUCCESS : SOURCE_BLOCKED;
        } catch (const Source<Out>::Unavailable&) {
            return SOURCE_UNAVAILABLE;
        } catch (const std::exception& exc) {
            m_exc = exc;
            return SOURCE_EXCEPTION;
        }
    }
};

}
