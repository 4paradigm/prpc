#include <cstdlib>
#include <glog/logging.h>
#include <gtest/gtest.h>

#include "URIConfig.h"

namespace paradigm4 {
namespace pico {
namespace core {

TEST(URIConfig, Parse) {
    URIConfig uri("hdfs://a/b/c?format=archive&compress=snappy&is_text=true");
    EXPECT_EQ(uri.storage_type(), FileSystemType::HDFS);
    EXPECT_EQ(uri.config().get<std::string>("format"), "archive");
    EXPECT_EQ(uri.config().get<std::string>("compress"), "snappy");
    EXPECT_EQ(uri.config().get<bool>("is_text"), true);

    auto u2 = uri + "/1";
    EXPECT_EQ(u2.name(), "hdfs://a/b/c/1");
    EXPECT_EQ(uri.name(), "hdfs://a/b/c");
    EXPECT_EQ(u2.config().get<std::string>("format"), "archive");
    EXPECT_EQ(u2.config().get<std::string>("compress"), "snappy");
    EXPECT_EQ(u2.config().get<bool>("is_text"), true);
}

TEST(URIConfig, SetLvl) {
    URIConfig uri("hdfs://path?a=00");
    uri.config().set_val("a", "0", 0);
    EXPECT_EQ(uri.config().get<std::string>("a"), "00");
    uri.config().set_val("a", "1", 1);
    EXPECT_EQ(uri.config().get<std::string>("a"), "1");
    uri.config().set_val("a", "3", 3);
    EXPECT_EQ(uri.config().get<std::string>("a"), "3");
}

TEST(URIConfig, Archive) {
    URIConfig uri("hdfs://path?a=00&b=01&c=2");
    BinaryArchive ar;
    ar << uri;
    URIConfig u;
    ar >> u;
    EXPECT_EQ(u.storage_type(), uri.storage_type());
    EXPECT_EQ(u.config().get<std::string>("a"), "00");
    EXPECT_EQ(u.config().get<std::string>("b"), "01");
    EXPECT_EQ(u.config().get<std::string>("c"), "2");
}

}
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret; 
}


