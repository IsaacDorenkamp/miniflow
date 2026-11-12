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
            Wirable* w = std::any_cast<Wirable>(&m_nodes.at(node));
            w->wire(predecessors, this);
        }
    }

    // next, create all interfaces
    for (uint32_t node : std::views::keys(m_graph.edgemap())) {
        predecessors = m_graph.predecessors(node);
        successors = m_graph.successors(node);
        if (predecessors.size() > 0 && successors.size() == 0) {
            // sink
            AbstractSink* sink = std::any_cast<AbstractSink>(&m_nodes.at(node));
            interface = sink->createSinkInterface(m_connectors.at(predecessors));
        } else if (predecessors.size() == 0 && successors.size() > 0) {
            // source
            AbstractSource* source = std::any_cast<AbstractSource>(&m_nodes.at(node));
            std::vector<std::any*> connectors;
            for (uint32_t successor : successors)
                connectors.push_back(&m_connectors.at(m_graph.predecessors(successor)));
            interface = source->createSourceInterface(connectors);
        } else if (predecessors.size() > 0 && successors.size() > 0) {
            // transform
            AbstractTransform* transform = std::any_cast<AbstractTransform>(&m_nodes.at(node));
            std::vector<std::any*> connectors;
            for (uint32_t successor : successors)
                connectors.push_back(&m_connectors.at(m_graph.predecessors(successor)));
            interface = transform->createTransformInterface(m_connectors.at(predecessors), connectors);
        } else {
            // TODO: more sophisticated logging
            std::cerr << "Found disconnected node " << node << " while preparing flow." << std::endl;
        }
    }

    m_prepared = true;
}

}
