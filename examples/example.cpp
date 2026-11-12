#include <iostream>

#include "miniflow.h" // IWYU pragma: keep

using namespace std;

class SimpleSource : public flow::Source<int> {
    int m_emitted;
public:
    SimpleSource() : m_emitted(0) {}
    int produce() {
        if (m_emitted == 10) throw Unavailable();
        return ++m_emitted;
    }
};

class SimpleTransform : public flow::Passthrough<int, int> {
public:
    virtual int transform(const int& val) {
        return 2 * val;
    }
};

class SimpleSink : public flow::Sink<int> {
public:
    void consume(const int& val) {
        cout << "value: " << val << endl;
    }
};

int main(void) {
    flow::Flow f;
    auto src = f.source<SimpleSource, int>();
    auto trn = f.transform<SimpleTransform, int, int>();
    auto snk = f.sink<SimpleSink, int>();
    src >> trn >> snk;
    src >> snk;
    f.prepare();
    f.execute();
    return 0;
}
