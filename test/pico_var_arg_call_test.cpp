#include <cstdlib>
#include <cstdio>
#include <memory>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "pico_test_common.h"
#include "pico_unittest_operator.h"
#include "common.h"

namespace paradigm4 {
namespace pico {

class Foo {
public:
    void call(std::function<void(int&, const int&)> func) {
        for (const auto& a : _nbrs) {
            func(_result, a); 
        }   
    }   
    void add_number_ptr(const int& a) {
        _nbrs.push_back(a);
    }   
    int result() {
        return _result;
    }   

private:
    std::vector<int> _nbrs;
    int _result = 0;
};

void add(int& a, const int& b) {
    a += b;
}

template<typename... C>
std::shared_ptr<Foo> test_call(C...cs) {
    auto p = std::pair<Foo*, decltype(&Foo::add_number_ptr)>(new Foo(),
            &Foo::add_number_ptr);
    return std::shared_ptr<Foo>(pico_var_arg_call(p, cs...).first);
}

TEST(PicoVarArgCall, test_ok) {
    {
        auto a = test_call(1,2,3,4);
        a->call(add);
        EXPECT_EQ(10, a->result());
    }
    {
        auto a = test_call();
        a->call(add);
        EXPECT_EQ(0, a->result());
    }
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
