#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <memory>
#include <iostream>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "macro.h"
#include "Factory.h"

namespace paradigm4 {
namespace pico {
namespace core {

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

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
