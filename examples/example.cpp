#include <iostream>

#include "miniflow.h" // IWYU pragma: keep

using namespace std;

class SimpleSource : public flow::Source<int> {
public:
    int produce() {
        return 1;
    }
};

class SimpleTransform : public flow::Transform<int, int> {
    int m_val;
public:
    void consume(const int& val) {
        m_val = val;
    }

    int produce() {
        return m_val * 2;
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
    return 0;
}
