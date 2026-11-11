#pragma once

#include <any>
#include <concepts> // IWYU pragma: keep
#include <cstdint>
#include <map>
#include <type_traits>

#include "dag.h"
#include "units.h"

#define DEFAULT_CAPACITY 4096

namespace flow {

class Flow {
    uint32_t m_idCounter;
    std::map<uint32_t, std::any> m_sourcesAndSinks;
    dag<uint32_t> m_graph;

    std::map<std::set<uint32_t>, std::any> m_connectors;

    template <typename T>
    Connector<T>& getOrCreateConnector(const std::set<uint32_t>& sources) {
        auto it = m_connectors.find(sources);
        if (it != m_connectors.end()) return it->second;
        return (m_connectors[sources] = Connector<T>(DEFAULT_CAPACITY));
    }
public:
    template <typename T>
    friend class SourceConnector;

    template <typename T>
    friend class SinkConnector;

    Flow() : m_idCounter(0), m_sourcesAndSinks(), m_graph() {}

    template <typename T, typename U, typename... Args, std::enable_if_t<
        std::conjunction<
            std::is_base_of<Source<U>, T>,
            std::is_constructible<T, Flow&, Args...>,
            std::is_copy_constructible<T>
        >::value, bool> = true
    >
    T& source(Args&&... args) {
        uint32_t id = m_idCounter++;
        T src(*this, std::forward(args)...);
        auto result = m_sourcesAndSinks.emplace(std::make_pair(id, src));
        result.first->second.m_flowId = id;
        if (result.second) m_graph.addNode(id);
        return SourceConnector<U>(id, this);
    }

    template <typename T, typename U, typename... Args, std::enable_if_t<
        std::conjunction<
            std::is_base_of<Sink<U>, T>,
            std::is_constructible<T, Flow&, Args...>,
            std::is_copy_constructible<T>
        >::value, bool> = true
    >
    T& sink(Args&&... args) {
        uint32_t id = m_idCounter++;
        T sink(*this, std::forward(args)...);
        auto result = m_sourcesAndSinks.emplace(std::make_pair(id, sink));
        result.first->second.m_flowId = id;
        if (result.second) m_graph.addNode(id);
        return result.second;
    }
};

template <typename T>
class SinkConnector {
    uint32_t m_flowId;
    Flow* m_flow;
public:
    SinkConnector(uint32_t flowId, Flow* flow) : m_flowId(flowId), m_flow(flow) {}
    uint32_t flowId() const noexcept {
        return m_flowId;
    }
};

template <typename T>
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
    uint32_t flowId() const noexcept {
        return m_flowId;
    }
};



}
