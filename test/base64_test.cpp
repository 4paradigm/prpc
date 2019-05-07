#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "pico_test_common.h"
#include "pico_unittest_operator.h"
#include "pico_log.h"
#include "Base64.h"

namespace paradigm4 {
namespace pico {

TEST(Base64, check_ok) {
    std::string text  = "picopicopicopicopicopicopicopicopicopicopicopicopicopicopi";
    std::string encoded;
    char decoded[1024];
    size_t decoded_len;
    pico_base64_encode(text.c_str(), text.length(), encoded);
    SLOG(INFO) << encoded;
    pico_base64_decode(encoded, decoded, decoded_len);
    ASSERT_EQ(text.length(), decoded_len);
    ASSERT_STREQ(text.c_str(), decoded);
}

} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    paradigm4::pico::test::PicoUnitTestCommon::singleton().initialize(&argc, argv);
    if (paradigm4::pico::test::PicoUnitTestOperator::singleton().is_show_operator()) {
        // no_wrapper, repeat_num=1
        paradigm4::pico::test::PicoUnitTestOperator::singleton().append(paradigm4::pico::test::NoWrapperOperator(1));
        paradigm4::pico::test::PicoUnitTestOperator::singleton().show_operator();
        paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
        return 0;
    }
    int ret = RUN_ALL_TESTS();
    paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
    return ret;
}
