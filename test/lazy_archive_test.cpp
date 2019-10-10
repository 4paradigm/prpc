#include <cstdlib>
#include <memory>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "LazyArchive.h"

namespace paradigm4 {
namespace pico {
namespace core {

int REPEAT = 100;
const int ALIGN = 32;
struct TestValue {
    char tmp[ALIGN] = {0};
} __attribute__((aligned(ALIGN)));

class mystring {
public:
    mystring(size_t size = 0) {
        _size = size / sizeof(TestValue) + 1;
        _data = reinterpret_cast<TestValue*>(pico_memalign(ALIGN, _size * ALIGN));
        _hold = std::shared_ptr<void>(reinterpret_cast<void*>(_data), pico_free);
        memset(static_cast<void*>(_data), 0, _size * ALIGN);
    }
    mystring(const mystring& s) {
        *this = s;
    }
    mystring& operator=(const mystring& s) {
        _size = s._size;
        _data = reinterpret_cast<TestValue*>(pico_memalign(ALIGN, _size * ALIGN));
        _hold = std::shared_ptr<void>(reinterpret_cast<void*>(_data), pico_free);
        memcpy(static_cast<void*>(_data), static_cast<void*>(s._data), _size * ALIGN);
        return *this;
    }
    mystring(mystring&&) = default;
    mystring& operator=(mystring&&) = default;
    char* c_str() {
        return reinterpret_cast<char*>(_data);
    }
    const char* c_str()const {
        return reinterpret_cast<const char*>(_data);
    }
    friend void pico_serialize(ArchiveWriter& ar, 
          SharedArchiveWriter& shared, mystring& s) {
        ar << s._size;
        shared.put_shared_uncheck(s._data, s._size);
    }
    friend void pico_deserialize(ArchiveReader& ar, 
          SharedArchiveReader& shared, mystring& s) {
        ar >> s._size;
        data_block_t own;
        shared.get_shared_uncheck(s._data, s._size, own);
        s._hold = std::make_shared<data_block_t>(std::move(own));
    };
    friend bool operator==(const mystring& a, const mystring& b) {
        EXPECT_EQ(reinterpret_cast<uintptr_t>(a.c_str()) % ALIGN, 0);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(b.c_str()) % ALIGN, 0);
        return strcmp(a.c_str(), b.c_str()) == 0;
    }
    friend bool operator!=(const mystring& a, const mystring& b) {
        return !(a == b);
    }
private:
    size_t _size;
    TestValue* _data;
    std::shared_ptr<void> _hold;
};


void mytest(bool use_apply) {
    std::vector<int> a = {1, 2, 3, 4}, a0 = a, a1;
    std::vector<double> b = {1.1, 2.1, 3.1, 4.1}, b0 = b, b1;
    BinaryArchive sub_ar, sub_ar0, sub_ar1;
    std::string tmp_str = "asdf", tmp_str0 = tmp_str, tmp_str1;
    sub_ar << tmp_str;
    std::string c = "dsagfgdbqotnlq", c0 = c, c1 = c;
    double d = 12345.6789, d0 = d, d1;
    std::pair<double, std::vector<int>> p = make_pair(d, a), p0 = p, p1;
    std::map<int32_t, std::vector<std::pair<int32_t, std::vector<float>>>> x = {
        {100, {
                {110, {111, 112, 113}},
                {120, {121}},
                {130, {121}},
        }},
        {200, {
                {210, {211, 212, 213}},
                {220, {}},
                {230, {231, 232, 233, 234, 235}},
        }},
        {300, {}},
    }, x0 = x, x1;
    std::vector<decltype(x)> y(REPEAT, x), y0 = y, y1;
    std::vector<decltype(x)> Y = y, Y0 = y, Y1(REPEAT);
    std::vector<mystring> z, z0, z1;
    for (int i = 0; i < REPEAT; i++) {
        mystring s(i);
        for (int j = 0; j < i; j++) {
            s.c_str()[j] = 'a' + (j * i) % 26;
        }
        z.push_back(s);
        z0.push_back(s);
    }
    std::vector<mystring> Z = z, Z0 = Z, Z1(REPEAT);
    if (use_apply) {
        std::shared_ptr<char> buffer;
        pico::core::vector<data_block_t> data;
        pico::core::vector<data_block_t> data2;
        std::unique_ptr<LazyArchive> hold;
        {
            LazyArchive ar;
            ar << std::move(a) << std::move(b) << std::move(c) << std::move(d)
               << std::move(p) << std::move(x) << std::move(y) << std::move(z)
               << std::move(sub_ar);
            for (int i = 0; i < REPEAT; i++) {
                ar << std::move(Y[i]) << std::move(Z[i]);
            }
            ar.apply(data);
            hold = std::make_unique<LazyArchive>(std::move(ar));
        }

        {
            for (auto& block : data) {
                data2.emplace_back(block.length);
                std::memcpy(data2.back().data, block.data, block.length);
            }
            LazyArchive ar;
            ar.attach(std::move(data2));
            ar >> a1 >> b1 >> c1 >> d1 >> p1 >> x1 >> y1 >> z1 >> sub_ar1;
            sub_ar1 >> tmp_str1;
            for (int i = 0; i < REPEAT; i++) {
                ar >> Y1[i] >> Z1[i];
            }
        }
    } else {
        LazyArchive ar;
        ar << std::move(a) << std::move(b) << std::move(c) << std::move(d)
           << std::move(p) << std::move(x) << std::move(y) << std::move(z)
           << std::move(sub_ar);
        for (int i = 0; i < REPEAT; i++) {
            ar << std::move(Y[i]) << std::move(Z[i]);
        }  
        ar >> a1 >> b1 >> c1 >> d1 >> p1 >> x1 >> y1 >> z1 >> sub_ar1;
        sub_ar1 >> tmp_str1;
        for (int i = 0; i < REPEAT; i++) {
            ar >> Y1[i] >> Z1[i];
        }
    }

    EXPECT_EQ(tmp_str0, tmp_str1);
    EXPECT_EQ(a0, a1);
    EXPECT_EQ(b0, b1);
    EXPECT_EQ(c0, c1);
    EXPECT_EQ(d0, d1);
    EXPECT_EQ(p0, p1);
    EXPECT_EQ(x0, x1);
    EXPECT_EQ(y0, y1);
    EXPECT_EQ(z0, z1);
    for (int i = 0; i < REPEAT; i++) {
        EXPECT_EQ(Y0[i], Y1[i]);
        EXPECT_EQ(Z0[i], Z1[i]);
    }
}

TEST(LazyArchive, lazy) {
    mytest(false);
}

TEST(LazyArchive, apply) {
    mytest(true);
}

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

