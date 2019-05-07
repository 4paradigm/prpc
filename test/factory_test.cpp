#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <memory>
#include <iostream>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "../include/macro.h"
#include "../include/pico_test_common.h"
#include "../include/pico_unittest_operator.h"
#include "../include/Factory.h"

namespace paradigm4 {
namespace pico {

template<class T>    
class Base : public Object {
    public:
    virtual std::string getName() {
        return "Base";
    }
};

class Foo1 : public Base<int> {
    public:
    std::string getName() {
        return "Foo1";
    }
};

template<class T>
class Foo2 : public Base<T> {
    public:
    T xx;
    std::string getName() {
        return "Foo2";
    }
};


PICO_DEFINE_FACTORY(foos);
PICO_FACTORY_REGISTER(foos, Foo1, foo1);
PICO_FACTORY_REGISTER(foos, Foo2<int>, foo2);


TEST(Factory, ok) {
    std::shared_ptr<Base<int>> foo1 = pico_foos_make_shared<Base<int>>("foo1");
    std::shared_ptr<Base<int>> foo2 = pico_foos_make_shared<Base<int>>("foo2");
    EXPECT_FALSE(foo1->getName() == foo2->getName());
    EXPECT_TRUE(foo1->getName() == "Foo1");
    EXPECT_TRUE(foo2->getName() == "Foo2");
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
    paradigm4::pico::test::PicoUnitTestCommon::singleton().finalize();
    return RUN_ALL_TESTS();
}
