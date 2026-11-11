#include <catch2/catch_test_macros.hpp>

#include <sstream>

#include "dag.h"

using flow::dag;
using std::set, std::string, std::stringstream, std::vector;

TEST_CASE("correctly identifies head nodes", "[dag]") {
    dag<int> d;
    for (int i = 1; i <= 5; i++) REQUIRE(d.addNode(i));

    set<int> heads = d.heads();
    for (int i = 1; i <= 5; i++) REQUIRE(heads.contains(i));

    REQUIRE(d.addEdge(2, 4));
    REQUIRE(d.addEdge(3, 5));
    heads = d.heads();
    REQUIRE(heads.contains(1));
    REQUIRE(heads.contains(2));
    REQUIRE(heads.contains(3));
    REQUIRE_FALSE(heads.contains(4));
    REQUIRE_FALSE(heads.contains(5));
}

TEST_CASE("enforces cycle prevention", "[dag]") {
    dag<int> d;
    for (int i = 1; i <= 5; i++) d.addNode(i);

    REQUIRE(d.addEdge(1, 2));
    REQUIRE(d.addEdge(2, 3));
    REQUIRE_FALSE(d.addEdge(3, 1));

    REQUIRE(d.addEdge(1, 4));
    REQUIRE(d.addEdge(4, 5));
    REQUIRE_FALSE(d.addEdge(5, 1));

    REQUIRE(d.addEdge(1, 5));
    REQUIRE_FALSE(d.addEdge(4, 1));
}

TEST_CASE("traverses all paths", "[dag]") {
    dag<int> d;
    for (int i = 1; i <= 5; i++) d.addNode(i);

    REQUIRE(d.addEdge(1, 2));
    REQUIRE(d.addEdge(2, 4));
    REQUIRE(d.addEdge(1, 3));
    REQUIRE(d.addEdge(3, 4));
    REQUIRE(d.addEdge(3, 5));

    int count = 0;
    std::vector<std::string> paths{"124", "134", "135"};
    d.traverse([&count, &paths](const std::vector<int>& path) {
        stringstream pathname;
        for (int i : path) pathname << i;
        string current = pathname.str();
        auto loc = std::find_if(paths.begin(), paths.end(), [&current](const string& check) { return check.compare(current) == 0; });
        if (loc != paths.end()) paths.erase(loc);
        count++;
    });
    REQUIRE(count == 3);
    REQUIRE(paths.empty());
}
