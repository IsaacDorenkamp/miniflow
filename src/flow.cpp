#include "flow.h"

#include <iostream>
#include <stdexcept>

#include "units.h"

namespace flow {

void Flow::prepare() {
    if (m_prepared) throw std::runtime_error("flow is already prepared!");

    std::set<uint32_t> predecessors;
    std::set<uint32_t> successors;
    std::unique_ptr<NodeInterface> interface;

    // first, create all connectors
    for (uint32_t node : std::views::keys(m_graph.edgemap())) {
        predecessors = m_graph.predecessors(node);
        if (predecessors.size() > 0) {
            Wirable* w = dynamic_cast<Wirable*>(m_nodes.at(node).get());
            w->wire(predecessors, this);
        }
    }

    // next, create all interfaces
    for (uint32_t node : std::views::keys(m_graph.edgemap())) {
        predecessors = m_graph.predecessors(node);
        successors = m_graph.successors(node);
        if (predecessors.size() > 0 && successors.size() == 0) {
            // sink
            AbstractSink* sink = dynamic_cast<AbstractSink*>(m_nodes.at(node).get());
            interface = sink->createSinkInterface(m_connectors.at(predecessors));
        } else if (predecessors.size() == 0 && successors.size() > 0) {
            // source
            AbstractSource* source = dynamic_cast<AbstractSource*>(m_nodes.at(node).get());
            std::vector<std::any*> connectors;
            for (uint32_t successor : successors)
                connectors.push_back(&m_connectors.at(m_graph.predecessors(successor)));
            interface = source->createSourceInterface(connectors);
        } else if (predecessors.size() > 0 && successors.size() > 0) {
            // transform
            AbstractTransform* transform = dynamic_cast<AbstractTransform*>(m_nodes.at(node).get());
            std::vector<std::any*> connectors;
            for (uint32_t successor : successors)
                connectors.push_back(&m_connectors.at(m_graph.predecessors(successor)));
            interface = transform->createTransformInterface(m_connectors.at(predecessors), connectors);
        } else {
            // TODO: more sophisticated logging
            std::cerr << "Found disconnected node " << node << " while preparing flow." << std::endl;
            continue;
        }
        m_interfaces.emplace(node, std::move(interface));
    }

    m_prepared = true;
}

}
