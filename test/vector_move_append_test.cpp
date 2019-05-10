#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "common.h"

namespace paradigm4 {
namespace pico {
namespace core {

TEST(VectorMoveAppend, ok) {
    std::vector<std::vector<int>> vect = {{3, 4}, {5}, {6, 7, 8}};
    std::vector<std::vector<int>> result = {{0}, {1, 2}};

    vector_move_append(result, vect);

    int i = 0;
    for (auto& v : result) {
        for (auto& e : v) {
            EXPECT_EQ(i++, e);
        }
    }

    EXPECT_EQ(vect.size(), 3u);

    for (auto& v : vect) {
        EXPECT_EQ(v.size(), 0u);
    }
}

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
