#pragma once

#include <stdexcept>
#include <map>
#include <set>
#include <vector>

namespace flow {

template <typename T>
class dag {
    std::map<T, std::set<T>> m_edges;
    std::set<T> m_heads;

    bool _search(const T& element, const T& from, std::set<T>& visited) {
        if (element == from) return true;
        auto adjacent = m_edges.find(from);
        if (adjacent == m_edges.end()) return false;
        for (const T& adj : adjacent->second) {
            if (visited.contains(adj)) continue;
            if (_search(element, adj, visited)) return true;
        }
        return false;
    }

    template <typename Fn> requires std::invocable<Fn, const std::vector<T>&>
    void _traverse(Fn processPath, const T& current, std::vector<T>& path) {
        auto successors = m_edges.find(current);
        if (successors == m_edges.end() || successors->second.size() == 0) {
            processPath(path);
            return;
        }
        for (const T& successor : successors->second) {
            path.push_back(successor);
            _traverse(processPath, successor, path);
            path.pop_back();
        }
    }
public:
    dag() : m_edges(), m_heads() {}

    bool search(const T& element, const T& from) {
        if (!m_edges.contains(from)) throw std::out_of_range("node does not exist");
        std::set<T> visited;
        return _search(element, from, visited);
    }

    bool addNode(const T& node) {
        auto result = m_edges.insert({ node, std::set<T>() });
        m_heads.insert(node);
        return result.second;
    }

    bool addEdge(const T& from, const T& to) {
        auto adjacent = m_edges.find(from);
        if (adjacent == m_edges.end()) return false;
        // if we can get to "from" by starting at "to", we have a cycle and thus
        // cannot insert the edge.
        if (search(from, to)) return false;
        std::set<T>& adjList = adjacent->second;
        bool success = adjList.insert(to).second;
        // "to" is no longer a head node
        if (success) {
            auto head = m_heads.find(to);
            if (head != m_heads.end()) m_heads.erase(head);
        }
        return success;
    }

    const std::set<T>& heads() const {
        return m_heads;
    }

    template <typename Fn> requires std::invocable<Fn, const std::vector<T>&>
    void traverse(Fn processPath) {
        std::vector<T> path;
        for (const T& head : m_heads) {
            path.push_back(head);
            _traverse(processPath, head, path);
            path.pop_back();
        }
    }
};

}
