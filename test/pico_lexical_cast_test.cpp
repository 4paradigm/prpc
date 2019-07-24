#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>
#include "common.h"
#include "pico_lexical_cast.h"


namespace paradigm4 {
namespace pico {
namespace core {

TEST(PicoCast, self_type) {
    std::string sstr = "Pico is called by ourselves.";
    std::string dstr;
    ASSERT_TRUE(pico_lexical_cast(sstr, dstr));
    EXPECT_STREQ(dstr.c_str(), sstr.c_str());
}

TEST(PicoCast, const_char_ptr_to_string) {
    const char* cstr = "Pico is called by ourselves.";
    std::string str;
    ASSERT_TRUE(pico_lexical_cast(cstr, str));
    EXPECT_STREQ(str.c_str(), cstr);
}

TEST(PicoCast, number_to_string) {
#define TEST_DEF(T) \
    { \
        T num_max = std::numeric_limits<T>::max(); \
        std::string str; \
        ASSERT_TRUE(pico_lexical_cast(num_max, str)); \
        EXPECT_STREQ(format_string("%d", num_max).c_str(), str.c_str()); \
        \
        T num_min = std::numeric_limits<T>::min(); \
        ASSERT_TRUE(pico_lexical_cast(num_min, str)); \
        EXPECT_STREQ(format_string("%d", num_min).c_str(), str.c_str()); \
        \
        T num_0 = 0; \
        ASSERT_TRUE(pico_lexical_cast(num_0, str)); \
        EXPECT_STREQ("0", str.c_str()); \
        \
    }

#define TEST_DEF_int8(T) \
    { \
        T num_max = std::numeric_limits<T>::max(); \
        std::string str; \
        ASSERT_TRUE(pico_lexical_cast(num_max, str)); \
        EXPECT_STREQ("127", str.c_str()); \
        \
        T num_min = std::numeric_limits<T>::min(); \
        ASSERT_TRUE(pico_lexical_cast(num_min, str)); \
        EXPECT_STREQ("-128", str.c_str()); \
        \
        T num_0 = 0; \
        ASSERT_TRUE(pico_lexical_cast(num_0, str)); \
        EXPECT_STREQ("0", str.c_str()); \
        \
    }

#define TEST_DEF_uint8(T) \
    { \
        T num_max = std::numeric_limits<T>::max(); \
        std::string str; \
        ASSERT_TRUE(pico_lexical_cast(num_max, str)); \
        EXPECT_STREQ("255", str.c_str()); \
        \
        T num_min = std::numeric_limits<T>::min(); \
        ASSERT_TRUE(pico_lexical_cast(num_min, str)); \
        EXPECT_STREQ("0", str.c_str()); \
        \
        T num_0 = 0; \
        ASSERT_TRUE(pico_lexical_cast(num_0, str)); \
        EXPECT_STREQ("0", str.c_str()); \
        \
    }

    TEST_DEF_int8(int8_t);
    TEST_DEF(int16_t);
    TEST_DEF(int32_t);
    TEST_DEF(int64_t);
    TEST_DEF_uint8(uint8_t);
    TEST_DEF(uint16_t);
    TEST_DEF(uint32_t);
    TEST_DEF(uint64_t);
    TEST_DEF(size_t);
#undef TEST_DEF
#undef TEST_DEF_int8
#undef TEST_DEF_uint8
}

TEST(PicoCast, string_to_number) {
#define TEST_DEF_TRUE(T, STR) \
    { \
        std::string str; \
        T num; \
        str = #STR; \
        ASSERT_TRUE(pico_lexical_cast(str, num)); \
        EXPECT_EQ(num, STR); \
        size_t len = str.size(); \
        ASSERT_TRUE(pico_lexical_cast(str + "123", num, len)); \
        EXPECT_EQ(num, STR); \
        ASSERT_TRUE(pico_lexical_cast(str + " 123", num, len)); \
        EXPECT_EQ(num, STR); \
    }

#define TEST_DEF_FALSE(T, STR) \
    { \
        std::string str; \
        T num; \
        str = #STR; \
        ASSERT_FALSE(pico_lexical_cast(str, num)); \
        size_t len = str.size(); \
        ASSERT_FALSE(pico_lexical_cast(str + "123", num, len)); \
        ASSERT_FALSE(pico_lexical_cast(str + " 123", num, len)); \
    }

#define TEST_DEF_U(T) \
    { \
        TEST_DEF_TRUE(T, 123); \
        TEST_DEF_FALSE(T, -123); \
    }

#define TEST_DEF_I(T) \
    { \
        TEST_DEF_TRUE(T, 123); \
        TEST_DEF_TRUE(T, -123); \
        TEST_DEF_TRUE(T, 0); \
        TEST_DEF_FALSE(T, 0.1); \
    }

    TEST_DEF_I(int8_t);
    TEST_DEF_I(int16_t);
    TEST_DEF_I(int32_t);
    TEST_DEF_I(int64_t);
    TEST_DEF_U(uint8_t);
    TEST_DEF_U(uint16_t);
    TEST_DEF_U(uint32_t);
    TEST_DEF_U(uint64_t);
    TEST_DEF_U(size_t);

#undef TEST_DEF_FALSE
#undef TEST_DEF_TRUE
#undef TEST_DEF_U
#undef TEST_DEF_I
}
TEST(PicoCast, string_to_bool) {
    bool res;
    ASSERT_TRUE(pico_lexical_cast("true", res));
    EXPECT_TRUE(res);
    ASSERT_TRUE(pico_lexical_cast("True", res));
    EXPECT_TRUE(res);
    ASSERT_TRUE(pico_lexical_cast("false", res));
    EXPECT_FALSE(res);
    ASSERT_TRUE(pico_lexical_cast("False", res));
    EXPECT_FALSE(res);
    ASSERT_FALSE(pico_lexical_cast("true1", res));
    ASSERT_FALSE(pico_lexical_cast("false1", res));
    ASSERT_FALSE(pico_lexical_cast("TRue", res));
    ASSERT_FALSE(pico_lexical_cast("FAlse", res));
}

TEST(PicoCast, const_char_ptr_to_number) {
    int32_t num;
    ASSERT_TRUE(pico_lexical_cast("12", num));
    EXPECT_EQ(num, 12);

    double d;
    float f;
    int8_t i8;
    int32_t i32;
    ASSERT_FALSE(pico_lexical_cast("", d));
    ASSERT_FALSE(pico_lexical_cast("", f));
    ASSERT_FALSE(pico_lexical_cast("", i8));
    ASSERT_FALSE(pico_lexical_cast("", i32));
}

TEST(PicoCast, string_to_floating) {
    std::vector<std::pair<std::string, double>> cases = {
        {"0.0", 0.0},
        {"-0.0", 0.0},
        {"1e-7", 1e-7},
        {"-1e-7", -1e-7},
        {"123.456", 123.456},
        {"0x0.0p+0", 0.0},
        {"0x0p+0", 0.0},
        {"-0x0p+0", 0.0},
        {"-0x0p-0", 0.0},
        {"-0x0p+100", 0.0},
        {"0x0p-100", 0.0},
        {"0x1.edd2f1a9fbe77p+6", 123.456},
        {"0x1.ad7f29abcaf48p-24", 1e-7},
        {"-0x1.12e0be826d695p-30", -1e-9},
        {"-0x1.4216ca930d75ap+186", -1.234e56},
        {"inf", std::numeric_limits<double>::infinity()},
        {"-inf", -std::numeric_limits<double>::infinity()}
    };
    double tmp;
    for (auto& item : cases) {
        ASSERT_TRUE(pico_lexical_cast(item.first, tmp));
        EXPECT_EQ(tmp, item.second);
        ASSERT_TRUE(pico_lexical_cast(item.first + "123", tmp, item.first.size()));
        EXPECT_EQ(tmp, item.second);
        ASSERT_TRUE(pico_lexical_cast(item.first + " 123", tmp, item.first.size()));
        EXPECT_EQ(tmp, item.second);
    }
    ASSERT_TRUE(pico_lexical_cast("nan", tmp));
    EXPECT_TRUE(std::isnan(tmp));
}

TEST(PicoCast, decimal_to_hex) {
    std::vector<std::pair<std::string, double>> cases = {
        {"0x0p+0", 0.0},
        {"0x1.edd2f1a9fbe77p+6", 123.456},
        {"0x1.ad7f29abcaf48p-24", 1e-7},
        {"-0x1.12e0be826d695p-30", -1e-9},
        {"-0x1.4216ca930d75ap+186", -1.234e56},
        {"nan", std::numeric_limits<double>::quiet_NaN()},
        {"inf", std::numeric_limits<double>::infinity()},
        {"-inf", -std::numeric_limits<double>::infinity()}

    };
    for (auto& item : cases) {
        std::string tmp;
        tmp = decimal_to_hex(item.second);
        EXPECT_EQ(tmp, item.first);
    }
}

TEST(PicoCast, pico_common_value_type_conversion_check) {
    double tmpd;
    float tmpf;
    int64_t tmpll;
    int32_t tmpi;
    int fvalue1 = 123;
    bool fvalue2 = true;
    std::string fvalue3 = "1234";
    std::string tvalue1;
    std::string tvalue2;
    int tvalue3;
    ASSERT_TRUE(pico_lexical_cast(fvalue1, tvalue1));
    ASSERT_TRUE(pico_lexical_cast(fvalue2, tvalue2));
    ASSERT_TRUE(pico_lexical_cast(fvalue3, tvalue3));
    EXPECT_TRUE(tvalue1 == "123");
    EXPECT_TRUE(tvalue2 == "true");
    EXPECT_TRUE(tvalue3 == 1234);
    ASSERT_TRUE(pico_lexical_cast<std::string>(fvalue1) == "123");
    ASSERT_TRUE(pico_lexical_cast<std::string>(fvalue2) == "true");
    ASSERT_TRUE(pico_lexical_cast<int>(fvalue3) == 1234);
    ASSERT_TRUE(pico_lexical_cast(fvalue3, tmpf));
    ASSERT_TRUE(pico_lexical_cast(fvalue3, tmpd));
    ASSERT_TRUE(pico_lexical_cast(fvalue1, tvalue3));
    ASSERT_TRUE(pico_lexical_cast(fvalue3, tvalue1));
    EXPECT_TRUE(fvalue1 == tvalue3);
    EXPECT_TRUE(fvalue3 == tvalue1);
    std::string errvalue = "123abc";
    ASSERT_FALSE(pico_lexical_cast<double>(errvalue, tmpd));
    errvalue = "1.23e+4932";
    ASSERT_FALSE(pico_lexical_cast<double>(errvalue, tmpd));
    errvalue = "100000000000000000000000000000000";
    ASSERT_FALSE(pico_lexical_cast<int64_t>(errvalue, tmpll));
    ASSERT_FALSE(pico_lexical_cast<int32_t>(errvalue, tmpi));
    std::string nan = "NAN";
    ASSERT_TRUE(std::isnan(pico_lexical_cast<float>(nan)));
    ASSERT_FALSE(pico_lexical_cast(nan, tmpll));
    nan = "-nan";
    EXPECT_TRUE(std::isnan(pico_lexical_cast<double>(nan)));
    ASSERT_FALSE(pico_lexical_cast(nan, tmpi));
    std::string inf = "inf";
    EXPECT_TRUE(std::isinf(pico_lexical_cast<float>(inf)));
    ASSERT_FALSE(pico_lexical_cast(inf, tmpll));
    inf = "-infinity";
    ASSERT_TRUE(std::isinf(pico_lexical_cast<double>(inf)));
    ASSERT_FALSE(pico_lexical_cast(inf, tmpi));
    std::string strip = "  123";
    ASSERT_FALSE(pico_lexical_cast(strip, tmpd));
    strip = "123  ";
    ASSERT_FALSE(pico_lexical_cast(strip, tmpf));
    strip = "1 23";
    ASSERT_FALSE(pico_lexical_cast(strip, tmpll));
    strip = ".1 2 3 ";
    ASSERT_FALSE(pico_lexical_cast(strip, tmpi));
    std::string real = "123.00";
    ASSERT_FALSE(pico_lexical_cast(real, tmpll));
    ASSERT_FALSE(pico_lexical_cast(real, tmpi));
    char hex[] = "0x123";
    ASSERT_FALSE(pico_lexical_cast(hex+0, tmpi));
    ASSERT_FALSE(pico_lexical_cast(hex+0, tmpll));
    char octal[] = "0123";
    ASSERT_TRUE(pico_lexical_cast<int32_t>(octal+0) == 123);
}



} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

