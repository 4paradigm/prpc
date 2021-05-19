#include <cstdlib>
#include <cstdio>
#include <queue>
#include <string>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "common.h"

namespace paradigm4 {
namespace pico {
namespace core {

TEST(commonTest, pico_common_time_relevent_function_check) {
    std::chrono::system_clock::time_point t0;
    char fmt[] = "%c %Z";
    std::string out_local = pico_format_time_point_local(t0, fmt);
    std::string ans_local = "Thu Jan  1 08:00:00 1970 CST";
    std::string out_gm = pico_format_time_point_gm(t0, fmt);
    std::string ans_gm = "Thu Jan  1 00:00:00 1970 GMT";
    EXPECT_EQ(out_local, ans_local);
    EXPECT_EQ(out_gm, ans_gm);

    auto init_time1 = pico_initialize_time();
    sleep(1);
    auto init_time2 = pico_initialize_time();
    sleep(1);
    auto cur_time1 = pico_current_time();
    sleep(1);
    auto cur_time2 = pico_current_time();
    EXPECT_TRUE(init_time1 == init_time2);
    EXPECT_FALSE(cur_time1 == cur_time2);
}

TEST(commonTest, pico_common_vectorappend_sgn_retry_check) {
    EXPECT_TRUE(IsVector<std::vector<int>>::value);
    EXPECT_FALSE(IsVector<int>::value);
    EXPECT_FALSE(IsVector<std::string>::value);
    EXPECT_FALSE(IsVector<std::deque<double>>::value);


    std::vector<int> result = {1, 2};
    std::vector<int> vect = {3, 4};
    std::vector<int> ans = {1, 2, 3, 4};
    vector_move_append(result, vect);
    EXPECT_EQ(result.size(), ans.size());
    for (size_t i = 0; i < ans.size(); ++i) {
        EXPECT_EQ(result[i], ans[i]);
    }

    EXPECT_EQ(sgn( 7),  1);
    EXPECT_EQ(sgn(-7), -1);
    EXPECT_EQ(sgn( 0),  0);
    EXPECT_EQ(sgn(7.77),  1);
    EXPECT_EQ(sgn(-7.7), -1);
    EXPECT_EQ(sgn(0.00),  0);

    double small_rand = -1;
    auto func = [](double& a) {
                a = pico_real_random();
                if (a < 0.2) {
                    return 0;
                }
                errno = EINTR;
                return -1;
            };
    EXPECT_EQ(retry_eintr_call(func, small_rand), 0);
    EXPECT_TRUE(small_rand < 0.2);
}

TEST(commonTest, pico_common_format_typename_hash_check) {
    std::string format = "test %d %.2f %s";
    std::string fmtret = format_string(format, 123, 7.77, "asdf1234");
    std::string fmtans = "test 123 7.77 asdf1234";
    EXPECT_TRUE(fmtret == fmtans);

    EXPECT_TRUE(readable_typename<int>() == "int");
    EXPECT_TRUE(readable_typename<bool>() == "bool");
    EXPECT_TRUE(readable_typename<float>() == "float");
    EXPECT_TRUE(readable_typename<std::string>() == 
        "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >");

    int32_t hash32_1 = pico_murmurhash3_32("key1");
    int32_t hash32_2 = pico_murmurhash3_32("key1");
    int32_t hash32_3 = pico_murmurhash3_32("key2");
    EXPECT_TRUE(hash32_1 == hash32_2);
    EXPECT_TRUE(hash32_1 != hash32_3);

    int64_t hash64_1 = pico_murmurhash3_64("long key 1");
    int64_t hash64_2 = pico_murmurhash3_64("long key 1");
    int64_t hash64_3 = pico_murmurhash3_64("long key 2");
    EXPECT_TRUE(hash64_1 == hash64_2);
    EXPECT_TRUE(hash64_1 != hash64_3);
}

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
