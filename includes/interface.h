#pragma once

namespace flow {

class NodeInterface {
public:
    virtual ~NodeInterface() = default;
    enum Result {
        SUCCESS,
        SOURCE_UNAVAILABLE,
        SOURCE_EXCEPTION,
        SOURCE_UNBLOCKED,  // when process() succeeds in resolving a blockage
        SOURCE_BLOCKED, // when the connector becomes blocked
        SINK_DEAD,
        SINK_EXCEPTION,
        SINK_WAITING
    };

    virtual Result process() = 0;
    virtual unsigned int lives() const = 0;
};

class AbstractNode {
public:
    virtual ~AbstractNode() = default;
};

}
