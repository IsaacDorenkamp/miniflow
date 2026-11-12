#pragma once

#include <any>
#include <concepts> // IWYU pragma: keep
#include <cstdint>
#include <map>
#include <memory>
#include <type_traits>

#include "dag.h"
#include "interface.h"

#define DEFAULT_CAPACITY 4096

namespace flow {

template <std::copy_constructible T>
class Connector;

template <std::copy_constructible T>
class Source;

template <std::copy_constructible T>
class Sink;

template <std::copy_constructible In, std::copy_constructible Out>
class Transform;

template <std::copy_constructible T>
class SourceConnector;

template <std::copy_constructible T>
class SinkConnector;

template <std::copy_constructible In, std::copy_constructible Out>
class TransformConnector;

class Flow {
    uint32_t m_idCounter;
    dag<uint32_t> m_graph;

    bool m_prepared;

    std::map<uint32_t, std::any> m_nodes;
    std::map<std::set<uint32_t>, std::any> m_connectors;
    std::map<uint32_t, std::unique_ptr<NodeInterface>> m_interfaces;

    template <std::copy_constructible T>
    Connector<T>& connectorFor(const std::set<uint32_t>& sources) {
        auto it = m_connectors.find(sources);
        if (it != m_connectors.end()) return *std::any_cast<Connector<T>>(&it->second);
        m_connectors.insert({sources, std::any(Connector<T>(DEFAULT_CAPACITY))});
        return *std::any_cast<Connector<T>>(&m_connectors.at(sources));
    }
public:
    template <std::copy_constructible T>
    friend class SourceConnector;

    template <std::copy_constructible In, std::copy_constructible Out>
    friend class TransformConnector;

    template <std::copy_constructible T>
    friend class SinkConnector;

    template <std::copy_constructible T>
    friend class Sink;

    Flow() : m_idCounter(0), m_graph(), m_prepared(false), m_nodes(), m_connectors() {}

    template <typename T, typename U, typename... Args, std::enable_if_t<
        std::conjunction<
            std::is_base_of<Source<U>, T>,
            std::is_constructible<T, Args...>,
            std::is_copy_constructible<T>
        >::value, bool> = true
    >
    SourceConnector<U> source(Args&&... args) {
        uint32_t id = m_idCounter++;
        T src(std::forward(args)...);
        auto result = m_nodes.emplace(std::make_pair(id, src));
        if (result.second) m_graph.addNode(id);
        return SourceConnector<U>(id, this);
    }

    template <typename T, typename U, typename... Args, std::enable_if_t<
        std::conjunction<
            std::is_base_of<Sink<U>, T>,
            std::negation<std::is_base_of<Source<U>, T>>,
            std::is_constructible<T, Args...>,
            std::is_copy_constructible<T>
        >::value, bool> = true
    >
    SinkConnector<U> sink(Args&&... args) {
        uint32_t id = m_idCounter++;
        T sink(std::forward(args)...);
        auto result = m_nodes.emplace(std::make_pair(id, sink));
        if (result.second) m_graph.addNode(id);
        return SinkConnector<U>(id, this);
    }

    template<typename T, typename In, typename Out, typename... Args, std::enable_if_t<
        std::conjunction<
            std::is_base_of<Source<Out>, T>,
            std::is_base_of<Sink<In>, T>,
            std::is_constructible<T, Args...>,
            std::is_copy_constructible<T>
        >::value, bool> = true
    >
    TransformConnector<In, Out> transform(Args&&... args) {
        uint32_t id = m_idCounter++;
        T transform(std::forward(args)...);
        auto result = m_nodes.emplace(std::make_pair(id, transform));
        if (result.second) m_graph.addNode(id);
        return TransformConnector<In, Out>(id, this);
    }

    void prepare();
};

template <std::copy_constructible T>
class SinkConnector {
    uint32_t m_flowId;
    Flow* m_flow;
public:
    SinkConnector(uint32_t flowId, Flow* flow) : m_flowId(flowId), m_flow(flow) {}
    uint32_t flowId() const noexcept {
        return m_flowId;
    }
};

template <std::copy_constructible In, std::copy_constructible Out>
class TransformConnector {
    uint32_t m_flowId;
    Flow* m_flow;
public:
    TransformConnector(uint32_t flowId, Flow* flow) : m_flowId(flowId), m_flow(flow) {}
    SinkConnector<Out>& operator>>(SinkConnector<Out>& into) {
        bool added = m_flow->m_graph.addEdge(flowId(), into.flowId());
        return into;
    }
    template <typename OtherOut>
    TransformConnector<Out, OtherOut>& operator>>(TransformConnector<Out, OtherOut>& into) {
        bool added = m_flow->m_graph.addEdge(flowId(), into.flowId());
        return into;
    }
    uint32_t flowId() const noexcept {
        return m_flowId;
    }
};

template <std::copy_constructible T>
class SourceConnector {
    uint32_t m_flowId;
    Flow* m_flow;
public:
    SourceConnector(uint32_t flowId, Flow* flow) : m_flowId(flowId), m_flow(flow) {}
    SinkConnector<T>& operator>>(SinkConnector<T>& into) {
        bool added = m_flow->m_graph.addEdge(flowId(), into.flowId());
        // FIXME: (not urgent but important) throw error if added is false
        return into;
    }
    template <std::copy_constructible OtherOut>
    TransformConnector<T, OtherOut>& operator>>(TransformConnector<T, OtherOut>& into) {
        bool added = m_flow->m_graph.addEdge(flowId(), into.flowId());
        return into;
    }
    uint32_t flowId() const noexcept {
        return m_flowId;
    }
};

}
