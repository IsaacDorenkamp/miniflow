#include <catch2/catch_test_macros.hpp>

#include <thread>

#include "ring.h"

using flow::ringbuffer, flow::prqueue;

static std::unique_ptr<ringbuffer<int>> g_buffer = nullptr;

ringbuffer<int>& setup_buffer(bool fill = true) {
    g_buffer.reset();
    g_buffer = std::make_unique<ringbuffer<int>>(4);
    if (fill)
        for (size_t i = 0; i < g_buffer->capacity(); i++)
            g_buffer->put_nowait(i);
    return *g_buffer;
}

TEST_CASE("single queue", "[ring]") {
    ringbuffer<int>& buffer = setup_buffer();
    prqueue<int> q = buffer.spawnqueue();
    for (int i = 0; i < buffer.capacity(); i++) REQUIRE(q.get_nowait() == i);
}

TEST_CASE("multiple queues", "[ring]") {
    ringbuffer<int>& buffer = setup_buffer();
    prqueue<int> q1 = buffer.spawnqueue();
    prqueue<int> q2 = buffer.spawnqueue();

    REQUIRE(q1.get_nowait() == 0);
    REQUIRE(q1.get_nowait() == 1);
    REQUIRE(q1.get_nowait() == 2);
    REQUIRE(q2.get_nowait() == 0);
    REQUIRE(q2.get_nowait() == 1);
    REQUIRE(q2.get_nowait() == 2);
    REQUIRE(buffer.length() == 1);

    REQUIRE(q2.get_nowait() == 3);
    REQUIRE(buffer.length() == 1);

    REQUIRE(q1.get_nowait() == 3);
    REQUIRE(buffer.length() == 0);
}

TEST_CASE("prqueue::get() with wait", "[ring]") {
    using namespace std::chrono_literals;

    ringbuffer<int>& buffer = setup_buffer();
    prqueue<int> q = buffer.spawnqueue();
    for (size_t i = 0; i < buffer.capacity(); i++) q.get_nowait();
    REQUIRE_THROWS_AS(q.get_nowait(), flow::empty);

    int i;
    std::thread t([&q, &i]() {
        i = q.get();
    });

    std::this_thread::sleep_for(25ms);

    buffer.put(123);
    t.join();
    REQUIRE(i == 123);
}

TEST_CASE("ringbuffer::put() with wait", "[ring]") {
    using namespace std::chrono_literals;

    ringbuffer<int>& buffer = setup_buffer();
    prqueue<int> q = buffer.spawnqueue();
    REQUIRE_THROWS_AS(buffer.put_nowait(123), flow::full);

    std::thread t([&buffer]() {
        buffer.put(123);
    });

    std::this_thread::sleep_for(25ms);

    q.get_nowait();
    t.join();
}

TEST_CASE("random interpolation of buffer and prqueue operations", "[ring]") {
    ringbuffer<int>& buffer = setup_buffer(false);
    prqueue<int> q1 = buffer.spawnqueue();

    buffer.put(123);
    buffer.put(456);

    int i = q1.get();
    REQUIRE(i == 123);

    prqueue<int> q2 = buffer.spawnqueue();
    i = q2.get();
    // q2 came late to the game, so its first item is 456
    REQUIRE(i == 456);

    buffer.put(789);
    buffer.put(321);

    i = q1.get();
    REQUIRE(i == 456);
    i = q1.get();
    REQUIRE(i == 789);
    i = q2.get();
    REQUIRE(i == 789);
    REQUIRE(buffer.length() == 1);
    i = q1.get();
    REQUIRE(i == 321);
    i = q2.get();
    REQUIRE(i == 321);
    REQUIRE(buffer.length() == 0);
}

TEST_CASE("prqueue with ephemeral lifetime", "[ring]") {
    ringbuffer<int>& buffer = setup_buffer();
    prqueue<int> q1 = buffer.spawnqueue();

    {
        prqueue<int> q2 = buffer.spawnqueue();
        q1.get();
        q1.get();
        REQUIRE(buffer.length() == 4);
    }

    REQUIRE(buffer.length() == 2);
    int i = q1.get();
    REQUIRE(i == 2);
}
