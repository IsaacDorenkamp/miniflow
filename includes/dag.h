#pragma once

#include <stdexcept>
#include <map>
#include <set>

namespace flow {


template <typename T>
class dag {
    std::map<T, std::set<T>> m_edges;
    std::set<T> m_heads;

    bool _search(const T& element, const T& from, std::set<T>& visited) {
        if (element == from) return true;
        auto adjacent = m_edges.find(from);
        if (adjacent == m_edges.end()) return false;
        for (const T& adj : adjacent.second) {
            if (visited.contains(adj)) continue;
            if (_search(element, adj)) return true;
        }
        return false;
    }
public:
    dag() : m_edges(), m_heads() {}

    bool search(const T& element, const T& from) {
        if (!m_edges.contains(from)) throw std::out_of_range("node does not exist");
        return _search(element, from);
    }

    bool addNode(const T& node) {
        auto result = m_edges.insert({ node, std::set<T>() });
        return result.second;
    }

    bool addEdge(const T& from, const T& to) {
        auto adjacent = m_edges.find(from);
        if (adjacent == m_edges.end()) return false;
        // if we can get to "from" by starting at "to", we have a cycle and thus
        // cannot insert the edge.
        if (search(from, to)) return false;
        bool success = adjacent->second.insert(to).second;
        // "to" is no longer a head node
        if (success) {
            auto head = m_heads.find(to);
            m_heads.erase(head);
        }
        return success;
    }

    template <typename Fn> requires std::invocable<Fn, const std::vector<T>&>
    void traversePaths(Fn processPath) {
        // TODO: generate all paths and call processPath()
    }
};

}
