#pragma once

#include <concepts>

#include "flow.h"
#include "interface.h"
#include "ring.h"

namespace flow {

class Flow;

template <typename T> requires std::copy_constructible<T>
class Sink;

template <typename T> requires std::copy_constructible<T>
class Source {
    friend class Flow;
    Flow* m_flow;
    uint32_t m_flowId;
public:
    Source(Flow& flow) : m_flow(&flow), m_flowId(0) {}
    Source(const Source& source) = default;
    virtual void produce(size_t amount, putcounter<T, ringbuffer<T>>& target) = 0;

    // a template is used here to allow objects which are both Source and Sink to receive redirection, and for
    // the type of the resulting expression to be identified with the exact type so as to allow it to be treated
    // as a Source even though it is also a Sink.
    template <typename U, std::enable_if_t<std::is_base_of<Sink<T>, U>::value> = true>
    U& operator>>(U& sink) {
        if (m_flow != sink.m_flow) throw std::runtime_error("Source and Sink must belong to the same Flow.");
        return sink;
    }

    uint32_t flowId() const noexcept {
        return m_flowId;
    }
};

template <typename T> requires std::copy_constructible<T>
class Sink {
    friend class Flow;
    friend class Source<T>;
    Flow* m_flow;
    uint32_t m_flowId;
public:
    Sink(Flow& flow) : m_flow(&flow), m_flowId(0) {}
    Sink(const Sink& sink) = default;
    virtual void consume(size_t amount, getcounter<T, prqueue<T>>& source) = 0;

    uint32_t flowId() const noexcept {
        return m_flowId;
    }
};

class Processor {
public:
    virtual size_t process(size_t maximumUnits) = 0;
};

template <typename T>
class SourceNode : public Processor {
    Source<T>* m_source;
    ringbuffer<T> m_buffer;
    putcounter<T, ringbuffer<T>> m_counter;
public:
    SourceNode(Source<T>& source, size_t buf_capacity) : m_source(&source), m_buffer(buf_capacity), m_counter(m_buffer) {}
    virtual size_t process(size_t maximumUnits) {
        maximumUnits = std::min(maximumUnits, m_buffer->capacity() - m_buffer->length());
        if (maximumUnits == 0) return 0;
        m_counter.reset();
        m_source->produce(maximumUnits, m_counter);
        return m_counter.count();
    }
};

template <typename T>
class SinkNode : public Processor {
    Sink<T>* m_sink;
    prqueue<T> m_sourceQueue;
    getcounter<T, prqueue<T>> m_counter;
public:
    SinkNode(Sink<T>& sink, prqueue<T>&& sourceQueue) : m_sink(&sink), m_sourceQueue(sourceQueue), m_counter(m_sourceQueue) {}
    virtual size_t process(size_t maximumUnits) {
        m_counter.reset();
        m_sink->consume(maximumUnits, m_counter);
        return m_counter.count();
    }
};

}
