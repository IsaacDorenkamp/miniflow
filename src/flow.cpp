#include "flow.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "units.h"
#include "queue.h"
#include "worker.h"

namespace flow {

void Flow::prepare() {
    if (m_prepared) throw std::runtime_error("Flow is already prepared!");
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

void Flow::execute() {
    if (!m_prepared) throw std::runtime_error("must call Flow::prepare() before Flow::execute()!");

    std::map<uint32_t, uint8_t> depths;
    std::map<uint32_t, uint8_t> lives;
    m_graph.traverse([this, &depths, &lives](const std::vector<uint32_t>& path) {
        uint8_t depth = 0;
        for (uint32_t node : path) {
            // it's ok to assign multiple times, subsequent assignments are idempotent
            lives[node] = m_interfaces[node]->lives();
            if (depth > depths[node]) depths[node] = depth;
            depth++;
        }
    });

    tqueue<uint32_t> pending;
    tqueue<Worker*> waitingLine;

    // should iterate from lowest to highest depth
    for (const std::pair<uint32_t, uint8_t>& entry : depths) {
        std::cout << "queuing node " << entry.first << " (depth " << std::to_string(entry.second) << ")" << std::endl;
        pending.put(entry.first);
    }

    // determine how many workers we need
    unsigned int maxThreads = std::thread::hardware_concurrency();
    if (maxThreads == 0) maxThreads = 4;

    unsigned int nWorkers = std::min(maxThreads, (unsigned int)pending.size());
    bool loyalWorkers = nWorkers <= pending.size();  // loyal workers will never try to change to a new worker

    std::vector<std::jthread> threads;
    for (unsigned int worker = 0; worker < nWorkers; worker++)
        threads.push_back(std::jthread(std::function<void(tqueue<Worker*>*)>(Worker(loyalWorkers)), &waitingLine));

    Worker* current;
    uint32_t id;
    while (lives.size() > 0) {
        current = waitingLine.get();
        if (pending.size() > 0) {
            bool requeue = true;
            if (current->m_died) {
                std::map<uint32_t, uint8_t>::iterator entry = lives.find(current->m_nodeId);
                entry->second--;
                if (entry->second == 0) {
                    lives.erase(entry);
                    // only prevent requeue if the node is *actually* dead
                    requeue = false;
                }
                current->m_died = false;
            }
            // put the node back in line for later processing
            if (requeue) pending.put(current->m_nodeId);

            id = pending.get();
            current->m_nodeId = id;
            current->m_interface = m_interfaces.at(id).get();
            current->m_waiter.notify_one();
        }
    }
}

}
