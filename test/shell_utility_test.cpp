#include <cstdlib>
#include <cstdio>

#include <gtest/gtest.h>

#include "pico_log.h"
#include "ShellUtility.h"

static int g_secret = 0;
static std::string g_tmp_root = "";

namespace paradigm4 {
namespace pico {
namespace core {

TEST(ShellUtility, execute_ok) {
    auto file = ShellUtility::execute("echo abc > /dev/null");
    SLOG(INFO) << file.get();
    EXPECT_TRUE(file != nullptr);
}

TEST(ShellUtility, execute_okk) {
    auto file = ShellUtility::execute("ls /ls > /dev/null; exit 0");
    EXPECT_TRUE(file != nullptr);
}

//TEST(ShellUtility, execute_failed) {
//    auto file = ShellUtility::execute("exit -1");
//    EXPECT_TRUE(file != nullptr);
//}

//TEST(ShellUtility, fail_external_kill) {
//    auto file = ShellUtility::execute("sleep 12345");
//    EXPECT_TRUE(file != nullptr);
//}

TEST(ShellUtility, get_command_output) {
    std::string cmd = "echo \"#include <cstdlib>\"";
    std::string output = ShellUtility::execute_tostring(cmd);
    EXPECT_STREQ(output.c_str(), "#include <cstdlib>\n");
}

TEST(ShellUtility, get_file_list) {
    std::string reg_path = format_string("%s/pico_*.%d", g_tmp_root, g_secret);

    std::vector<std::string> file_list = ShellUtility::get_file_list(reg_path);
    sort(file_list.begin(), file_list.end());

    EXPECT_EQ(4u, file_list.size());
    for(int i = 0; i < 4; ++i) {
        EXPECT_STREQ(file_list[i].c_str(), 
                format_string("%s/pico_%d.%d", g_tmp_root, i, g_secret).c_str());
    }
}

TEST(ShellUtility, bipopen) {
    auto p = ShellUtility::bipopen("awk '{print $1 * 2, $2 * 3}'");
    for (int i = 0; i < 100; ++i) {
        fprintf(p.fp_write, "%d %d\n", i, i+1);
    }

    fclose(p.fp_write);

    for (int i = 0; i < 100; ++i) {
        int a, b;
        EXPECT_TRUE(fscanf(p.fp_read, "%d %d\n", &a, &b) == 2);
        EXPECT_EQ(a, i * 2);
        EXPECT_EQ(b, (i + 1) * 3);
    }

    fclose(p.fp_read);

    int result = ShellUtility::bipclose(p);
    EXPECT_EQ(result, 0);
}

}
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    using paradigm4::pico::core::ShellUtility;
    using paradigm4::pico::core::format_string;
    testing::InitGoogleTest(&argc, argv);
    srand(time(NULL));
    g_secret = rand();
    g_tmp_root = format_string("./.unittest_tmp/ShellUtility.%d.%d", g_secret, getpid());
    ShellUtility::execute(
            format_string("rm -rf %s && mkdir -p %s", g_tmp_root, g_tmp_root));
    ShellUtility::execute(
            format_string("touch %s/pico_0.%d", g_tmp_root, g_secret));
    ShellUtility::execute(
            format_string("touch %s/pico_1.%d", g_tmp_root, g_secret));
    ShellUtility::execute(
            format_string("touch %s/pico_2.%d", g_tmp_root, g_secret));
    ShellUtility::execute(
            format_string("touch %s/pico_3.%d", g_tmp_root, g_secret));
    int ret = RUN_ALL_TESTS();
    ShellUtility::execute(format_string("rm -rf %s", g_tmp_root));
    return ret;
}
