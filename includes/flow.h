#pragma once

#include <set>
#include <span>

namespace {

template <typename T>
class LPtr;

template <typename T>
class LPtrCtrl {
public:
    friend class LPtr<T>;

    LPtrCtrl(T* instance) : instance(instance) {}
    virtual ~LPtrCtrl() {
        for (LPtr<T>* ptr : ptrs) {
            ptr->ctrl = nullptr;
            ptr->m_instance = nullptr;
        }
    }
    T* instance;
    std::set<LPtr<T>*> ptrs;
};

template <typename T>
class LPtr {
public:
    LPtr(LPtrCtrl<T>& ctrl) : m_ctrl(&ctrl), m_instance(ctrl.instance), m_key(ctrl.instance) {
        ctrl.ptrs.insert(this);
    }
    virtual ~LPtr() {
        if (m_ctrl) {
            typename std::set<LPtr<T>*>::const_iterator entry = m_ctrl->ptrs.find(this);
            if (entry != m_ctrl->ptrs.end()) m_ctrl->ptrs.erase(entry);
        }
    }

    operator bool() const {
        return m_instance != nullptr;
    }

    T& operator*() {
        return *m_instance;
    }

    const T& operator*() const {
        return *m_instance;
    }

    T* operator->() {
        return m_instance;
    }

    const T* operator->() const {
        return m_instance;
    }

    bool operator==(const LPtrCtrl<T>& other) const {
        return m_key == other.m_key;
    }

    bool operator<(const LPtrCtrl<T>& other) const {
        return m_key < other.m_key;
    }
private:
    LPtrCtrl<T>* m_ctrl;
    T* m_instance;
    uintptr_t m_key;
};

}

namespace flow {

template <typename T>
class Window {
public:
    using Container = std::vector<T>;

    class Lock {
    public:
    };

    Window();
    virtual ~Window() = default;
};

template <typename T>
class Sink {
public:
    Sink() : m_ctrl(this) {}
    virtual ~Sink() = default;
    virtual size_t accept(std::span<T>& span) = 0;
private:
    LPtrCtrl<Sink> m_ctrl;
};

template <typename T>
class Source {
public:
    using LPtrSet = std::set<LPtr<Sink<T>>>;

    friend class Sink<T>;
    Source() : m_sinks() {}
    virtual ~Source() = default;
    virtual size_t available() const = 0;
    virtual size_t produce(std::span<T>& span) = 0;
    virtual bool done() = 0;

    Sink<T>& operator>>(Sink<T>& sink) {
        using std::pair;
        pair<typename LPtrSet::iterator, bool> result = m_sinks.insert(LPtr<Sink<T>>(sink.m_ctrl));
        return *result.first;
    }
private:
    LPtrSet m_sinks;
};

template <typename In, typename Out>
class Connector {
public:
    Connector(Source<In>& source, Sink<Out>& sink) : m_transit(), m_source(source), m_sink(sink) {}
    void operator()() {
        size_t avail = m_source->available();
        m_transit.resize();
        std::span<Out> outSpan(m_transit.begin(), m_transit.end());
        size_t produced = m_source->produce(outSpan);
        std::span<Out> sinkSpan(m_transit.begin(), m_transit.begin() + produced);
        m_sink->accept(sinkSpan);
    }
private:
    std::vector<Out> m_transit;
    Source<In>* m_source;
    Sink<Out>* m_sink;
};

class Flow {
public:
    Flow();
};

}
