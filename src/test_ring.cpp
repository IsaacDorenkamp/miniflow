#include <catch2/catch_test_macros.hpp>

#include "ring.h"

using flow::ringvec;

TEST_CASE("basic ringvec tests") {
    ringvec<int> test(8);

    // Test that we can successfully insert to all positions
    for (int i = 0; i < 8; i++) REQUIRE(test.push_back(i));

    // Ensure length is properly updated
    REQUIRE(test.length() == 8);

    // Ensure all elements have the expected value
    for (int i = 0; i < 8; i++) REQUIRE(test[i] == i);

    // Ensure that we cannot push when full
    REQUIRE_FALSE(test.push_back(9));

    // Ensure that we can pop elements, and that doing so allows
    // us to push new values.
    REQUIRE(test.pop_front());
    REQUIRE(test.length() == 7);
    REQUIRE(test.push_back(9));

    // Ensure that a queue whose front has been changed still
    // indexes correctly
    REQUIRE(test[0] == 1);
}

TEST_CASE("test ringvec iterator") {
    ringvec<int> test(8);
    for (int i = 0; i < 8; i++) test.push_back(i);

    int shouldBe = 0;
    for (int element : test) {
        REQUIRE(element == shouldBe++);
    }
}
