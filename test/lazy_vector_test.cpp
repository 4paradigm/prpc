#include <memory>
#include <thread>
#include <atomic>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "gflags/gflags.h"
#include "pico_test_common.h"
#include "pico_unittest_operator.h"
#include "LazyVector.h"

namespace paradigm4 {
namespace pico {

    TEST(LazyVector, constructor_ok) {
        LazyVector<int> lazy_vector;
        EXPECT_EQ(0u, lazy_vector.size());
        EXPECT_EQ(true, lazy_vector.begin() == lazy_vector.end());
        EXPECT_TRUE(lazy_vector.begin() == lazy_vector.end());
    }

    TEST(LazyVector, push_back_ok) {
        LazyVector<int> lazy_vector;
        EXPECT_EQ(0u, lazy_vector.size());
        lazy_vector.push_back(1);
        EXPECT_EQ(1u, lazy_vector.size());
        lazy_vector.push_back(3);
        EXPECT_EQ(2u, lazy_vector.size());
    }

    TEST(LazyVector, clear_ok) {
        LazyVector<int> lazy_vector;
        EXPECT_EQ(0u, lazy_vector.size());
        lazy_vector.push_back(1);
        EXPECT_EQ(1u, lazy_vector.size());
        lazy_vector.push_back(3);
        EXPECT_EQ(2u, lazy_vector.size());
        lazy_vector.clear();
        EXPECT_EQ(0u, lazy_vector.size());
        lazy_vector.push_back(2);
        EXPECT_EQ(1u, lazy_vector.size());
        lazy_vector.clear();
        EXPECT_EQ(0u, lazy_vector.size());
        lazy_vector.push_back(4);
        EXPECT_EQ(1u, lazy_vector.size());
    }

    TEST(LazyVector, square_bracket_ok) {
        LazyVector<int> lazy_vector;
        EXPECT_EQ(0u, lazy_vector.size());
        lazy_vector.push_back(1);
        EXPECT_EQ(1u, lazy_vector.size());
        lazy_vector.push_back(3);
        EXPECT_EQ(2u, lazy_vector.size());
        lazy_vector.push_back(5);
        lazy_vector.push_back(8);
        lazy_vector.push_back(10);
        EXPECT_EQ(5, lazy_vector[2]);
        EXPECT_EQ(10, lazy_vector[4]);
        EXPECT_EQ(1, lazy_vector[0]);
//        EXPECT_EQ(lazy_vector[0], lazy_vector[2000]);
        lazy_vector.clear();
        EXPECT_EQ(0u, lazy_vector.size());
//        EXPECT_EQ(lazy_vector[0], lazy_vector[2000]);
    }

    TEST(LazyVector, self_define_type_ok) {
        struct elem_t {
            int64_t i64;
            double d;
            int8_t i8;
        };

        LazyVector<elem_t> lazy_vector;
        for (int i = 0; i < 10; ++i) {
            elem_t e = {i, (double) i, (int8_t) i};
            lazy_vector.push_back(e);
        }

        EXPECT_EQ(10u, lazy_vector.size());
        EXPECT_EQ(3, lazy_vector[3].i64);
        EXPECT_EQ(3, lazy_vector[3].i8);

        EXPECT_EQ(8, lazy_vector[8].i64);
        EXPECT_EQ(8, lazy_vector[8].i8);
    }

    TEST(LazyVector, multiple_views_ok) {
        struct elem_t {
            int64_t i64;
            double d;
            int8_t i8;
        };
        LazyVector<elem_t> lazy_vector(100);
        for (int i = 0; i < 1000; ++i) {
            elem_t e = {i * 2, (double) i - 0.5, (int8_t) (i % 100)};
            lazy_vector.push_back(e);
        }
        EXPECT_EQ(1000u, lazy_vector.size());
        for (int i = 0; i < 1000; ++i) {
            EXPECT_EQ(i * 2, lazy_vector[i].i64);
            EXPECT_EQ((int8_t) (i % 100), lazy_vector[i].i8);
        }
    }

    TEST(LazyVector, shrink_ok) {
        LazyVector<int> lazy_vector(100);
        for (int i = 0; i< 50000; i++) {
            lazy_vector.push_back(i * 3);
        }
        EXPECT_EQ(50000u, lazy_vector.size());
        EXPECT_EQ(15000, lazy_vector[5000]);
        EXPECT_EQ(0, lazy_vector[0]);
        EXPECT_EQ(149997, lazy_vector[49999]);
//        EXPECT_EQ(0, lazy_vector[100000]);
        lazy_vector.shrink();
        EXPECT_EQ(50000u, lazy_vector.size());
        EXPECT_EQ(15000, lazy_vector[5000]);
        EXPECT_EQ(0, lazy_vector[0]);
        EXPECT_EQ(149997, lazy_vector[49999]);
//        EXPECT_EQ(0, lazy_vector[100000]);
    }

    TEST(LazyVector, cut_ok) {
        LazyVector<int> lazy_vector(100);
        for (int i = 0; i < 2000; ++i) {
        lazy_vector.push_back(i);
        }
        EXPECT_EQ(2000u, lazy_vector.size());
        EXPECT_EQ(1500, lazy_vector[1500]);

        LazyVector<int> tail(200);
        lazy_vector.cut(1200, tail);
        EXPECT_EQ(1200u, lazy_vector.size());
        EXPECT_EQ(1000, lazy_vector[1000]);
        EXPECT_EQ(1199, lazy_vector[1199]);
//        EXPECT_EQ(0, lazy_vector[1200]);
//        EXPECT_EQ(0, lazy_vector[1400]);

        EXPECT_EQ(800u, tail.size());
        EXPECT_EQ(1200, tail[0]);
        EXPECT_EQ(1500, tail[300]);
        EXPECT_EQ(1999, tail[799]);
//        EXPECT_EQ(1200, tail[800]);

        lazy_vector.cut(0, tail);
        EXPECT_EQ(0u, lazy_vector.size());
    }

    TEST(LazyVector, double_cut_ok) {
        LazyVector<int> lazy_vector(100);
        for (int i = 0; i < 2000; ++i) {
            lazy_vector.push_back(i);
        }
        EXPECT_EQ(2000u, lazy_vector.size());
        EXPECT_EQ(1500, lazy_vector[1500]);

        LazyVector<int> tail(200);
        lazy_vector.cut(1200, tail);
        EXPECT_EQ(1000, lazy_vector[1000]);
        EXPECT_EQ(1199, lazy_vector[1199]);
        EXPECT_EQ(1999, tail[799]);
        EXPECT_EQ(800u, tail.size());
//        EXPECT_EQ(1200, tail[800]);

        LazyVector<int> tail_tail(300);
        tail.cut(500, tail_tail);
        EXPECT_EQ(1699, tail[499]);
        EXPECT_EQ(500u, tail.size());
//        EXPECT_EQ(1200, tail[500]);
        EXPECT_EQ(1700, tail_tail[0]);
        EXPECT_EQ(1999, tail_tail[299]);
        EXPECT_EQ(300u, tail_tail.size());
//        EXPECT_EQ(1700, tail_tail[500]);
    }

    TEST(LazyVector, merge_ok) {
        LazyVector<int> lazy_vector(100);
        for (int i = 0; i < 1000; ++i) {
            lazy_vector.push_back(i);
        }
        LazyVector<int> lazy_vector2(100);
        for (int i = 2000; i < 3000; ++i) {
            lazy_vector2.push_back(i);
        }
        lazy_vector.merge(std::move(lazy_vector2));
        EXPECT_EQ(2000u, lazy_vector.size());
        EXPECT_EQ(500, lazy_vector[500]);
        EXPECT_EQ(999, lazy_vector[999]);
        EXPECT_EQ(2500, lazy_vector[1500]);
        EXPECT_EQ(2999, lazy_vector[1999]);
        EXPECT_EQ(0, lazy_vector[0]);
    }

    TEST(LazyVector, sub_ok) {
        LazyVector<int> lazy_vector(200);
        for (int i = 0; i < 2000; ++i) {
            lazy_vector.push_back(i);
        }
        EXPECT_EQ(2000u, lazy_vector.size());
        EXPECT_EQ(1500, lazy_vector[1500]);

        LazyVector<int> sub(100);
        sub = lazy_vector.sub(200,700);
        EXPECT_EQ(2000u, lazy_vector.size());
        EXPECT_EQ(300, lazy_vector[300]);
        EXPECT_EQ(500u, sub.size());
        EXPECT_EQ(200, sub[0]);
        EXPECT_EQ(300, sub[100]);
        EXPECT_EQ(699, sub[499]);
//        EXPECT_EQ(200, sub[500]);

        sub = lazy_vector.sub(700,200);
        EXPECT_EQ(0u, sub.size());
    }

    TEST(LazyVector, cut_merge_ok) {
        LazyVector<int> lv1(200);
        for (int i = 0; i < 2000; ++i) {
            lv1.push_back(i);
        }
        LazyVector<int> lv2(300);
        lv1.cut(100, lv2);
        EXPECT_EQ(100u, lv1.size());
        EXPECT_EQ(1900u, lv2.size());

        LazyVector<int> lv3(50);
        lv2.cut(100, lv3);
        EXPECT_EQ(100u, lv2.size());
        EXPECT_EQ(1800u, lv3.size());
        EXPECT_EQ(300, lv3[100]);

        lv2.merge(std::move(lv1));
        EXPECT_EQ(200u, lv2.size());
        EXPECT_EQ(0, lv2[100]);
        EXPECT_EQ(99, lv2[199]);

        lv3.merge(std::move(lv2));
        EXPECT_EQ(2000u, lv3.size());
        EXPECT_EQ(1000, lv3[800]);
        EXPECT_EQ(100, lv3[1800]);
        EXPECT_EQ(0, lv3[1900]);
        EXPECT_EQ(200, lv3[0]);
    }

    TEST(LazyVector, cut_merge_sub_ok) {
        LazyVector<int> lv1(200);
        for (int i = 0; i < 2000; ++i) {
            lv1.push_back(i);
        }
        LazyVector<int> lv2(300);
        lv1.cut(100, lv2);
        EXPECT_EQ(100u, lv1.size());
        EXPECT_EQ(1900u, lv2.size());

        LazyVector<int> lv_sub(500);
        lv_sub = lv2.sub(100, 1100);
        EXPECT_EQ(1000u, lv_sub.size());
        EXPECT_EQ(200, lv_sub[0]);
        EXPECT_EQ(1199, lv_sub[999]);
//        EXPECT_EQ(200, lv_sub[1000]);

        LazyVector<int> lv3(50);
        lv2.cut(400, lv3);
        EXPECT_EQ(400u, lv2.size());
        EXPECT_EQ(1500u, lv3.size());
        EXPECT_EQ(300, lv2[200]);

        EXPECT_EQ(500, lv3[0]);
        EXPECT_EQ(1999, lv3[1499]);
//        EXPECT_EQ(500, lv3[1500]);

        lv3.shrink();

        lv2.merge(std::move(lv1));
        EXPECT_EQ(500u, lv2.size());
        EXPECT_EQ(0, lv2[400]);
        EXPECT_EQ(99, lv2[499]);

        lv3.merge(std::move(lv_sub));
        EXPECT_EQ(2500u, lv3.size());
        EXPECT_EQ(1000, lv3[500]);
        EXPECT_EQ(200, lv3[1500]);
        EXPECT_EQ(1199, lv3[2499]);
        EXPECT_EQ(500, lv3[0]);

        lv3.merge(std::move(lv2));
        EXPECT_EQ(3000u, lv3.size());
        EXPECT_EQ(0, lv3[2900]);

        LazyVector<int> lv4(150);
        lv3.cut(1500, lv4);
        lv4.merge(std::move(lv3));
        EXPECT_EQ(3000u, lv4.size());
        EXPECT_EQ(800, lv4[1800]);
    }

    TEST(LazyVector, iterator_ok) {
        LazyVector<int> lv1(200);
        for (int i = 0; i < 2000; ++i) {
            lv1.push_back(i);
        }

        int cnt = 0;
        for (auto item: lv1) {
            EXPECT_EQ(cnt++, item);
        }

        LazyVector<int> lv2(300);
        lv1.cut(100, lv2);

        LazyVector<int> lv_sub(500);
        lv_sub = lv2.sub(100, 1100);

        LazyVector<int> lv3(50);
        lv2.cut(100, lv3);

        cnt = 100;
        for (auto item: lv2) {
            EXPECT_EQ(cnt++, item);
        }
        EXPECT_EQ(cnt, lv2.size() + 100);

        cnt = 200;
        for (auto item: lv_sub) {
            EXPECT_EQ(cnt++, item);
        }
        EXPECT_EQ(cnt, 1200);

        lv2.shrink();

        cnt = 100;
        for (auto item: lv2) {
            EXPECT_EQ(cnt++, item);
        }
        EXPECT_EQ(cnt, lv2.size() + 100);

        lv1.clear();
        EXPECT_EQ(lv1.begin(), lv1.end());

    }

    TEST(LazyVector, iterator_sub_ok) {
        LazyVector<int> lv1(200);
        for (int i = 0; i < 2000; ++i) {
            lv1.push_back(i);
        }

        int cnt = 0;
        auto begin = lv1.begin();
        auto end = lv1.end();
        for (auto it = lv1.begin(); it != lv1.end(); it++) {
            if (cnt == 333)
                begin = it;
            if (cnt == 1111)
                end = it;
            cnt++;
        }

        LazyVector<int> lv_sub(150);
        lv_sub = lv1.sub(begin, end);

        cnt = 333;
        for (auto item: lv_sub) {
            EXPECT_EQ(cnt++, item);
        }
        EXPECT_EQ(cnt, 1111);
        EXPECT_EQ(778u, lv_sub.size());

    }

} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    paradigm4::pico::test::PicoUnitTestCommon::singleton().initialize(&argc, argv);
    if (paradigm4::pico::test::PicoUnitTestOperator::singleton().is_show_operator()) {
        // no_wrapper, repeat_num=100
        paradigm4::pico::test::PicoUnitTestOperator::singleton().append(paradigm4::pico::test::NoWrapperOperator(100));
        paradigm4::pico::test::PicoUnitTestOperator::singleton().show_operator();
        paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
        return 0;
    }
    paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
    return RUN_ALL_TESTS();
}

