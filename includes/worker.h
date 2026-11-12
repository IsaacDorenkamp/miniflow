#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "interface.h"
#include "queue.h"

namespace flow {

class Worker {
    std::mutex m_mutex;
    std::condition_variable m_waiter;
    NodeInterface* m_interface;
    bool m_waiting;
    bool m_loyal;
    std::chrono::milliseconds m_waitTime;
    
    // used by Flow::execute() to get details about why and how work stopped on its current interface
    int m_nodeId;
    bool m_died;
public:
    friend class Flow;

    Worker(bool loyal, std::chrono::milliseconds waitTime = std::chrono::milliseconds(25)) : m_mutex(), m_waiter(), m_interface(nullptr),
        m_waiting(false), m_loyal(loyal), m_waitTime(waitTime), m_nodeId(-1), m_died(false) {}
    Worker(const Worker& other) : m_mutex(), m_waiter(), m_interface(other.m_interface), m_waiting(other.m_waiting),
        m_loyal(other.m_loyal), m_waitTime(other.m_waitTime), m_nodeId(-1), m_died(other.m_died) {}

    void operator()(tqueue<Worker*>* waitingLine) {
        using namespace std::chrono_literals;
        bool running = true;
        bool processing = true;
        while (running) {
            std::unique_lock lk(m_mutex);
            if (!m_interface) {
                m_waiting = true;
                waitingLine->put(this);
                m_waiter.wait(lk, [this](){ return !m_waiting; });
                if (!m_interface) break;
            }

            processing = true;
            while (processing) {
                NodeInterface::Result result = m_interface->process();
                switch (result) {
                case NodeInterface::SOURCE_UNBLOCKED:
                case NodeInterface::SUCCESS:
                    break;
                case NodeInterface::SOURCE_UNAVAILABLE:
                    // TODO: inform the orchestrator that this source should be shutdown
                    processing = false;
                    m_interface = nullptr;
                    m_died = true;
                    break;
                case NodeInterface::SOURCE_BLOCKED:
                    if (!m_loyal) {
                        processing = false;
                        m_interface = nullptr;
                    }
                    break;
                case NodeInterface::SOURCE_EXCEPTION:
                case NodeInterface::SINK_EXCEPTION:
                    // TODO: log/audit and handle exception
                    // otherwise, keep processing until we no longer can
                    break;
                case NodeInterface::SINK_DEAD:
                    // TODO: inform the orchestrator that this sink should be shutdown
                    processing = false;
                    m_interface = nullptr;
                    m_died = true;
                    break;
                case NodeInterface::SINK_WAITING:
                    if (!m_loyal) {
                        processing = false;
                        m_interface = nullptr;  // if a sink is waiting for input, we should leave it
                    } else {
                        std::this_thread::sleep_for(m_waitTime);
                    }
                    break;
                }
            }

            // if we are loyal and our interface is no more, then we should stop
            if (m_loyal && !m_interface) running = false;
        }
    }
};

}
