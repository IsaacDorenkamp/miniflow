#pragma once

#include <concepts>  // IWYU pragma: keep
#include <exception>
#include <optional>
#include <queue>
#include <typeindex>

#include "flow.h"
#include "interface.h"
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

template <std::copy_constructible T, typename U, std::enable_if_t<std::is_base_of<Source<T>, U>::value, bool> = true>
class SourceInterface : public virtual NodeInterface {
    U* m_source;
    std::vector<Connector<T>*> m_sinks;
    std::queue<Connector<T>*> m_stuck;
    std::optional<std::exception> m_exc;
public:
    SourceInterface(U& source, const std::vector<Connector<T>*>& outgoingConnectors) : m_source(&source), m_sinks(outgoingConnectors), m_stuck(), m_exc() {}
    Result process() {
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
        } catch (const typename Source<T>::Unavailable& exc) {
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
class SinkInterface : public virtual NodeInterface {
    U* m_sink;
    prqueue<T> m_queue;
    std::optional<std::exception> m_exc;
public:
    SinkInterface(U& sink, Connector<T>& connector) : m_sink(&sink), m_queue(connector.observe()) {}
    Result process() {
        T element = m_queue.get();
        try {
            m_sink->consume(element);
        } catch (const std::exception& exc) {
            m_exc = exc;
            return SINK_EXCEPTION;
        }
        return SUCCESS;
    }

    const std::optional<std::exception>& exception() const {
        return m_exc;
    }
};

template <std::copy_constructible In, std::copy_constructible Out, typename U,
         std::enable_if_t<std::is_base_of<Transform<In, Out>, U>::value, bool> = true>
class TransformInterface : public SourceInterface<Out, Source<Out>>, public SinkInterface<In, Sink<In>> {
public:
    TransformInterface(U& transform, Connector<In>& incomingConnector, const std::vector<Connector<Out>*>& outgoingConnectors)
        : SourceInterface<Out, Source<Out>>(transform, outgoingConnectors), SinkInterface<In, Sink<In>>(transform, incomingConnector) {}
    NodeInterface::Result process() final {
        NodeInterface::Result result = this->SinkInterface<In, Sink<In>>::process();
        if (result != NodeInterface::SUCCESS) return result;
        return this->SourceInterface<Out, Source<Out>>::process();
    }
};

class AbstractSource : public virtual AbstractNode {
public:
    virtual ~AbstractSource() = default;
    virtual std::unique_ptr<NodeInterface> createSourceInterface(const std::vector<std::any*>& outgoingConnectors) = 0;
};

template <std::copy_constructible T>
class Source : public AbstractSource {
public:
    virtual ~Source() = default;

    class Unavailable : public std::exception {};
    virtual T produce() = 0;

    std::unique_ptr<NodeInterface> createSourceInterface(const std::vector<std::any*>& outgoingConnectors) final {
        std::vector<Connector<T>*> connectors;
        for (std::any* item : outgoingConnectors)
            connectors.push_back(std::any_cast<Connector<T>>(item));
        return std::unique_ptr<NodeInterface>(new SourceInterface<T, Source<T>>(*this, connectors));
    }
};

template <std::copy_constructible T>
class Connector {
    std::optional<T> m_stuck; 
    ringbuffer<T> m_buffer;
public:
    Connector(size_t capacity) : m_stuck(), m_buffer(capacity) {}
    virtual ~Connector() = default;

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

class Wirable {
public:
    virtual ~Wirable() = default;
    virtual void wire(const std::set<uint32_t>& predecessors, Flow* flow) = 0;
};

class AbstractSink : public Wirable, public virtual AbstractNode {
public:
    virtual ~AbstractSink() = default;
    virtual std::unique_ptr<NodeInterface> createSinkInterface(std::any& connector) = 0;
};

template <std::copy_constructible T>
class Sink : public AbstractSink {
public:
    virtual ~Sink() = default;

    virtual void consume(const T& element) = 0;
    void wire(const std::set<uint32_t>& predecessors, Flow* flow) final {
        flow->connectorFor<T>(predecessors);
    }
    std::unique_ptr<NodeInterface> createSinkInterface(std::any& connector) final {
        return std::unique_ptr<NodeInterface>(new SinkInterface<T, Sink<T>>(*this, std::any_cast<Connector<T>&>(connector)));
    }
};

class AbstractTransform : public virtual AbstractNode {
public:
    virtual ~AbstractTransform() = default;
    virtual std::unique_ptr<NodeInterface> createTransformInterface(std::any& incomingConnector, const std::vector<std::any*>& outgoingConnectors) = 0;
};

template <std::copy_constructible In, std::copy_constructible Out>
class Transform : public Sink<In>, public Source<Out>, public AbstractTransform {
public:
    virtual ~Transform() = default;
    std::unique_ptr<NodeInterface> createTransformInterface(std::any& incomingConnector, const std::vector<std::any*>& outgoingConnectors) final {
        std::vector<Connector<Out>*> connectors;
        for (std::any* connector : outgoingConnectors)
            connectors.push_back(std::any_cast<Connector<Out>>(connector));
        TransformInterface<In, Out, Transform<In, Out>>* interface
            = new TransformInterface<In, Out, Transform<In, Out>>(*this, *std::any_cast<Connector<In>>(&incomingConnector), connectors);
        return std::unique_ptr<NodeInterface>((NodeInterface*)interface);
    }
};

}
