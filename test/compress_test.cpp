#include <cstdlib>
#include <cstdio>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "common.h"
#include "Compress.h"

namespace paradigm4 {
namespace pico {

struct Foo {
    std::string data;
    size_t size;
    std::vector<std::string> haha;
    PICO_SERIALIZATION(data, size);
};

TEST(compress, snappy_string_ok) {
    auto c = pico_compress("snappy");
    for (int i = 0; i < 1; ++i) {
        Foo foo;
        foo.data = "I am GENIUS!!";
        foo.size = foo.data.length();
        std::string ret;
        c.compress(foo, ret);
        Foo foo2;
        c.uncompress(ret, foo2);
        EXPECT_TRUE(foo2.data == foo.data);
        EXPECT_TRUE(foo2.size == foo.size);
    }
}

TEST(compress, zlib_string_ok) {
    auto c = pico_compress("zlib");
    for (int i = 0; i < 20; ++i) {
        Foo foo;
        foo.data = "I am GENIUS!!";
        foo.size = foo.data.length();
        std::string ret;
        c.compress(foo, ret);
        Foo foo2;
        c.uncompress(ret, foo2);
        EXPECT_TRUE(foo2.data == foo.data);
        EXPECT_TRUE(foo2.size == foo.size);
    }
}

TEST(compress, lz4_string_ok) {
    auto c = pico_compress("lz4");
    for (int i = 0; i < 20; ++i) {
        Foo foo;
        foo.data = "I am GENIUS!!asdfasdfasdfasdfasfdasdfasdfasfasdfasda";
        foo.size = foo.data.length();
        foo.haha.resize(10);
        std::string ret;
        c.compress(foo, ret);
        Foo foo2;
        c.uncompress(ret, foo2);
        EXPECT_TRUE(foo2.data == foo.data);
        EXPECT_TRUE(foo2.size == foo.size);
    }
}

TEST(compress, snappy_raw_buffer_ok) {
    char* buffer = nullptr;
    size_t buffer_size = 0;
    auto c = pico_compress("snappy");
    for (int i = 0; i < 1; ++i) {
        Foo foo;
        foo.data = "I am GENIUS!!";
        foo.size = foo.data.length();
        c.compress(foo, &buffer, &buffer_size);
        Foo foo2;
        c.uncompress(buffer, buffer_size, foo2);
        EXPECT_TRUE(foo2.data == foo.data);
        EXPECT_TRUE(foo2.size == foo.size);
    }
}

TEST(compress, zlib_raw_buffer_ok) {
    char* buffer = nullptr;
    size_t buffer_size = 0;
    auto c = pico_compress("zlib");
    for (int i = 0; i < 1; ++i) {
        Foo foo;
        foo.data = "I am GENIUS!!";
        foo.size = foo.data.length();
        c.compress(foo, &buffer, &buffer_size);
        Foo foo2;
        c.uncompress(buffer, buffer_size, foo2);
        EXPECT_TRUE(foo2.data == foo.data);
        EXPECT_TRUE(foo2.size == foo.size);
    }
}


TEST(compress, lz4_raw_buffer_ok) {
    char* buffer = nullptr;
    size_t buffer_size = 0;
    auto c = pico_compress("lz4");
    for (int i = 0; i < 1; ++i) {
        Foo foo;
        foo.data = "I am GENIUS!!";
        foo.size = foo.data.length();
        c.compress(foo, &buffer, &buffer_size);
        Foo foo2;
        c.uncompress(buffer, buffer_size, foo2);
        EXPECT_TRUE(foo2.data == foo.data);
        EXPECT_TRUE(foo2.size == foo.size);
    }
}



TEST(compress, snappy_buffer_ok) {
    auto c = pico_compress("snappy");
    BinaryArchive ar;
    for (int i = 0; i < 20; ++i) {
        Foo foo, foo2;
        foo.data = "I am GENIUS!!";
        foo.size = foo.data.length();
        c.compress(foo, ar);
        c.uncompress(ar, foo2);
        EXPECT_TRUE(foo2.data == foo.data);
        EXPECT_TRUE(foo2.size == foo.size);
    }
}


TEST(compress, zlib_buffer_ok) {
    auto c = pico_compress("zlib");
    BinaryArchive ar;
    for (int i = 0; i < 20; ++i) {
        Foo foo, foo2;
        foo.data = "I am GENIUS!!";
        foo.size = foo.data.length();
        c.set_property("compress_level", "4");
        c.compress(foo, ar);
        c.uncompress(ar, foo2);
        EXPECT_TRUE(foo2.data == foo.data);
        EXPECT_TRUE(foo2.size == foo.size);
    }
}

TEST(compress, lz4_buffer_ok) {
    auto c = pico_compress("lz4");
    BinaryArchive ar;
    for (int i = 0; i < 20; ++i) {
        Foo foo, foo2;
        foo.data = "I am GENIUS!!";
        foo.size = foo.data.length();
        c.compress(foo, ar);
        c.uncompress(ar, foo2);
        EXPECT_TRUE(foo2.data == foo.data);
        EXPECT_TRUE(foo2.size == foo.size);
    }
}



TEST(compress, snappy_raw_ok) {
    BinaryArchive in, out;
    auto c = pico_compress("snappy");
    for (int i = 0; i < 20; ++i) {
        Foo foo, foo2;
        foo.data = "I am GENIUS!!";
        foo.size = foo.data.length();
        in.clear();
        in << foo;
        c.raw_compress(in, out);
        c.raw_uncompress(out, in);
        in >> foo2;
        EXPECT_TRUE(foo2.data == foo.data);
        EXPECT_TRUE(foo2.size == foo.size);
    }

    for (int i = 0; i < 20; ++i) {
        Foo foo, foo2;
        foo.data = "I am GENIUS!!";
        foo.size = foo.data.length();
        in.clear();
        in << foo;
        char* c1 = nullptr;
        size_t size1 = 0;
        c.raw_compress((char*)in.cursor(), in.readable_length(), &c1, &size1);
        char* c2 = nullptr;
        size_t size2 = 0;
        c.raw_uncompress(c1, size1, &c2, &size2);
        BinaryArchive ar;
        ar.set_read_buffer(c2, size2);
        ar >> foo2;
        ar.release();
        pico_free(c1);
        pico_free(c2);
        EXPECT_TRUE(foo2.data == foo.data);
        EXPECT_TRUE(foo2.size == foo.size);
    }

}

TEST(compress, zlib_raw_ok) {
    BinaryArchive in, out;
    auto c = pico_compress("zlib");
    for (int i = 0; i < 20; ++i) {
        Foo foo, foo2;
        foo.data = "I am GENIUS!!";
        foo.size = foo.data.length();
        in.clear();
        in << foo;
        c.raw_compress(in, out);
        c.raw_uncompress(out, in);
        in >> foo2;
        EXPECT_TRUE(foo2.data == foo.data);
        EXPECT_TRUE(foo2.size == foo.size);
    }
    char* c1 = nullptr;
    size_t size1 = 0;
    char* c2 = nullptr;
    size_t size2 = 0;
    for (int i = 0; i < 20; ++i) {
        Foo foo, foo2;
        foo.data = "I am GENIUS!!";
        foo.size = foo.data.length();
        in.clear();
        in << foo;
        c.raw_compress((char*)in.cursor(), in.readable_length(), &c1, &size1);
        c.raw_uncompress(c1, size1, &c2, &size2);
        BinaryArchive ar;
        ar.set_read_buffer(c2, size2);
        ar >> foo2;
        ar.release();
        EXPECT_TRUE(foo2.data == foo.data);
        EXPECT_TRUE(foo2.size == foo.size);
    }
    pico_free(c1);
    pico_free(c2);
}

} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
