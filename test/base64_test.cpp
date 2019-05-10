#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "pico_log.h"
#include "Base64.h"

namespace paradigm4 {
namespace pico {
namespace core {

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

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
