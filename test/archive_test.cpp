#include <cstdlib>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "Archive.h"

namespace paradigm4 {
namespace pico {

class Foo{};
class FooSer{};
bool pico_serialize(BinaryArchive&, const FooSer&) {
    return true;
}

bool pico_serialize(TextArchive&, const FooSer&) {
    return true;
}

bool pico_serialize(BinaryFileArchive&, const FooSer&) {
    return true;
}

bool pico_serialize(TextFileArchive&, const FooSer&) {
    return true;
}


TEST(BinaryFile, set_buffer) {
    int x = 12, y = 15;
    BinaryArchive ar;
    typedef BinaryArchive::allocator_type allocator_type;
    allocator_type alloc;
    ar.set_write_buffer(
          alloc.allocate(12), 12, [&](void* ptr) { alloc.deallocate((char*)ptr, 1); });
    char* c = alloc.allocate(15);
    ar.set_write_buffer(c, 15);
    ar << x << y;
    BinaryArchive ar1;
    ar1.set_read_buffer(c, 15,  [&](void* ptr) { alloc.deallocate((char*)ptr, 1); });
    ar.release();
    int x1, y1;
    ar1 >> x1 >> y1;
    BinaryArchive ar3;
    ar3.assign(c, sizeof(int) * 2);
    int x2, y2;
    ar3 >> x2 >> y2;
    EXPECT_TRUE(x1 == x);
    EXPECT_TRUE(y1 == y);
    EXPECT_TRUE(x2 == x);
    EXPECT_TRUE(y2 == y);
}

TEST(BinaryArchive, IsBinaryArchivable) {
    EXPECT_FALSE(IsBinarySerializable<Foo>::value);
    EXPECT_FALSE(IsBinaryDeserializable<Foo>::value);
    EXPECT_FALSE(IsBinaryArchivable<Foo>::value);

    EXPECT_TRUE(IsBinarySerializable<FooSer>::value);
    EXPECT_FALSE(IsBinaryDeserializable<FooSer>::value);
    EXPECT_FALSE(IsBinaryArchivable<FooSer>::value);

    EXPECT_TRUE(IsBinarySerializable<std::string>::value);
    EXPECT_TRUE(IsBinaryDeserializable<std::string>::value);
    EXPECT_TRUE(IsBinaryArchivable<int>::value);
}

TEST(TextArchive, IsTextArchivable) {
    EXPECT_FALSE(IsTextSerializable<Foo>::value);
    EXPECT_FALSE(IsTextDeserializable<Foo>::value);
    EXPECT_FALSE(IsTextArchivable<Foo>::value);

    EXPECT_TRUE(IsTextSerializable<FooSer>::value);
    EXPECT_FALSE(IsTextDeserializable<FooSer>::value);
    EXPECT_FALSE(IsTextArchivable<FooSer>::value);

    EXPECT_TRUE(IsTextSerializable<std::string>::value);
    EXPECT_TRUE(IsTextDeserializable<std::string>::value);
    EXPECT_TRUE(IsTextArchivable<int>::value);
}

TEST(BinaryFileArchive, IsBinaryFileArchivable) {
    EXPECT_FALSE(IsBinaryFileSerializable<Foo>::value);
    EXPECT_FALSE(IsBinaryFileDeserializable<Foo>::value);
    EXPECT_FALSE(IsBinaryFileArchivable<Foo>::value);

    EXPECT_TRUE(IsBinaryFileSerializable<FooSer>::value);
    EXPECT_FALSE(IsBinaryFileDeserializable<FooSer>::value);
    EXPECT_FALSE(IsBinaryFileArchivable<FooSer>::value);

    EXPECT_TRUE(IsBinaryFileSerializable<std::string>::value);
    EXPECT_TRUE(IsBinaryFileDeserializable<std::string>::value);
    EXPECT_TRUE(IsBinaryFileArchivable<int>::value);
}

TEST(TextFileArchive, IsTextFileArchivable) {
    EXPECT_FALSE(IsTextFileSerializable<Foo>::value);
    EXPECT_FALSE(IsTextFileDeserializable<Foo>::value);
    EXPECT_FALSE(IsTextFileArchivable<Foo>::value);

    EXPECT_TRUE(IsTextFileSerializable<FooSer>::value);
    EXPECT_FALSE(IsTextFileDeserializable<FooSer>::value);
    EXPECT_FALSE(IsTextFileArchivable<FooSer>::value);

    EXPECT_TRUE(IsTextFileSerializable<std::string>::value);
    EXPECT_TRUE(IsTextFileDeserializable<std::string>::value);
    EXPECT_TRUE(IsTextFileArchivable<int>::value);
}

TEST(BinaryArchive, archive_int) {
    BinaryArchive ar;
    int a  = 1;
    ar << a;

    int b ;
    ar >> b;
    EXPECT_EQ(a, b);
}

TEST(TextArchive, archive_int) {
    TextArchive ar;
    int a  = 1;
    ar << a;

    int b ;
    ar >> b;
    EXPECT_EQ(a, b);
}

TEST(BinaryFileArchive, archive_int) {
    FILE* file = std::tmpfile();
    BinaryFileArchive ar(file);
    int a  = 1;
    ar << a;

    std::rewind(file);

    int b ;
    ar >> b;
    EXPECT_EQ(a, b);

    fclose(file);
}

TEST(TextFileArchive, archive_int) {
    FILE* file = std::tmpfile();
    TextFileArchive ar(file);
    int a  = 1;
    ar << a;

    std::rewind(file);

    int b ;
    ar >> b;
    EXPECT_EQ(a, b);

    fclose(file);
}

TEST(Archive, empty_vector) {
    std::vector<std::string> a;
    BinaryArchive ar1;
    ar1 << a;
    ar1 >> a;
    EXPECT_EQ(a.size(), 0);

    TextArchive ar2;
    ar2 << a;
    ar2 >> a;
    EXPECT_EQ(a.size(), 0);

    FILE* file = std::tmpfile();
    BinaryFileArchive ar3(file);
    ar3 << a;
    std::rewind(file);
    ar3 >> a;
    EXPECT_EQ(a.size(), 0);
    fclose(file);
    
    file = std::tmpfile();
    BinaryFileArchive ar4(file);
    ar4 << a;
    std::rewind(file);
    ar4 >> a;
    EXPECT_EQ(a.size(), 0);
    fclose(file);
}

TEST(BinaryArchive, archive_vector) {
    BinaryArchive ar;
    std::vector<std::string> a = {"adsf", "c", "", "\n", " "};
    std::vector<bool> c = {true, true, false};
    std::vector<bool> c1;
    std::vector<int8_t> c2 = {0, 1, 0, 1, 1};
    ar << a << c << c1 << c2;

    std::vector<std::string> b;
    std::vector<bool> d;
    std::vector<bool> d1 = {true, true};
    std::vector<int8_t> d2;
    ar >> b >> d >> d1 >> d2;

    EXPECT_EQ(a.size(), b.size());
    EXPECT_EQ(c.size(), d.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_STREQ(a[i].c_str(), b[i].c_str());
    }
 
    for (size_t i = 0; i < c.size(); ++i) {
        EXPECT_TRUE(c[i] == d[i]);
    }

    EXPECT_EQ(d1.size(), 0u);

    EXPECT_EQ(c2.size(), d2.size());
    for (size_t i = 0; i < c2.size(); ++i) {
        EXPECT_EQ(c2[i], d2[i]);
    }
}

TEST(BinaryArchive, archive_bool_vector) {
    BinaryArchive ar;
    std::vector<bool> v;
    v.assign(500, false);
    v[0] = true; v[100] = true;
    ar << v;
    v.clear();
    ar >> v;
    int tot = 0;
    for (auto b : v) tot += b;
    EXPECT_EQ(tot, 2);

    v.assign(500, true);
    v[0] = false; v[100] = false;
    ar << v;
    v.clear();
    ar >> v;
    tot = 0;
    for (auto b : v) tot += b;
    EXPECT_EQ(tot, 498);
}

TEST(TextArchive, archive_vector) {
    TextArchive ar;
    std::vector<std::string> a = {"adsf", "c", "", "\n", " "};
    std::vector<int8_t> a2 = {1, 0, 0, 1, 1};
    ar << a << a2;
    static_assert(std::is_same<int, signed int>::value, "error");

    std::vector<std::string> b;
    std::vector<int8_t> b2;
    ar >> b >> b2;

    EXPECT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_STREQ(a[i].c_str(), b[i].c_str());
    }
    
    EXPECT_EQ(a2.size(), b2.size());
    for (size_t i = 0; i < a2.size(); ++i) {
        EXPECT_EQ(a2[i], b2[i]);
    }
}

TEST(BinaryFileArchive, archive_vector) {
    FILE* file = std::tmpfile();
    BinaryFileArchive ar(file);
    std::vector<std::string> a = {"adsf", "c", "", "\n", " "};
    ar << a;

    std::rewind(file);

    std::vector<std::string> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_STREQ(a[i].c_str(), b[i].c_str());
    }

    fclose(file);
}

TEST(TextFileArchive, archive_vector) {
    FILE* file = std::tmpfile();
    TextFileArchive ar(file);
    std::vector<std::string> a = {"adsf", "c", "", "\n", " "};
    std::vector<int8_t> a2 = {1, 0, 0, 1, 1};
    std::vector<char> a3 = {'a', ' ', '\0', 'B'};
    ar << a << a2 << a3;

    std::rewind(file);

    std::vector<std::string> b;
    std::vector<int8_t> b2;
    std::vector<char> b3;
    ar >> b;
    ar >> b2;
    ar >> b3;
    EXPECT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_STREQ(a[i].c_str(), b[i].c_str());
    }

    EXPECT_EQ(a2.size(), b2.size());
    for (size_t i = 0; i < a2.size(); ++i) {
        EXPECT_EQ(a2[i], b2[i]);
    }

    EXPECT_EQ(a3.size(), b3.size());
    for (size_t i = 0; i < a3.size(); ++i) {
        EXPECT_EQ(a3[i], b3[i]);
    }

    fclose(file);
}

TEST(BinaryArchive, archive_valarray) {
    BinaryArchive ar;
    std::valarray<int> a = {1, 2, 3, 4, 5};
    ar << a;

    std::valarray<int> b;
    ar >> b;

    EXPECT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
}

TEST(TextArchive, archive_valarray) {
    TextArchive ar;
    std::valarray<int> a = {1, 2, 3, 4, 5};
    ar << a;

    std::valarray<int> b;
    ar >> b;

    EXPECT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
}

TEST(BinaryFileArchive, archive_valarray) {
    FILE* file = std::tmpfile();
    BinaryFileArchive ar(file);
    std::valarray<int> a = {1, 2, 3, 4, 5};
    ar << a;

    std::rewind(file);

    std::valarray<int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i], b[i]);
    }

    fclose(file);
}

TEST(TextFileArchive, archive_valarray) {
    FILE* file = std::tmpfile();
    TextFileArchive ar(file);
    std::valarray<int> a = {1, 2, 3, 4, 5};
    ar << a;

    std::rewind(file);

    std::valarray<int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i], b[i]);
    }

    fclose(file);
}

TEST(BinaryArchive, archive_tuple) {
    BinaryArchive ar;
    std::tuple<std::string, int, float> a = std::make_tuple("0", 1, 0.2);
    ar << a;

    std::tuple<std::string, int, float> b;
    ar >> b;
    EXPECT_EQ(a, b);
}

TEST(TextArchive, archive_tuple) {
    TextArchive ar;
    std::tuple<std::string, int, float> a = std::make_tuple("0", 1, 0.2);
    ar << a;

    std::tuple<std::string, int, float> b;
    ar >> b;
    EXPECT_EQ(a, b);
}

TEST(BinaryFileArchive, archive_tuple) {
    FILE* file = std::tmpfile();
    BinaryFileArchive ar(file);
    std::tuple<std::string, int, float> a = std::make_tuple("0", 1, 0.2);
    ar << a;

    std::rewind(file);

    std::tuple<std::string, int, float> b;
    ar >> b;
    EXPECT_EQ(a, b);

    fclose(file);
}

TEST(TextFileArchive, archive_tuple) {
    FILE* file = std::tmpfile();
    TextFileArchive ar(file);
    std::tuple<std::string, int, long> a = std::make_tuple("0", 1, 1234567);
    ar << a;

    std::rewind(file);

    std::tuple<std::string, int, long> b;
    ar >> b;
    EXPECT_EQ(a, b);

    fclose(file);
}

TEST(BinaryArchive, archive_map) {
    BinaryArchive ar;
    std::map<int, int> a;

    for (int i = 0; i < 10; ++i) {
        a[i] = rand();
    }

    ar << a;
    std::map<int, int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
}

TEST(TextArchive, archive_map) {
    TextArchive ar;
    std::map<int, int> a;

    for (int i = 0; i < 10; ++i) {
        a[i] = rand();
    }

    ar << a;
    std::map<int, int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
}

TEST(BinaryFileArchive, archive_map) {
    FILE* file = std::tmpfile();
    BinaryFileArchive ar(file);
    std::map<int, int> a;

    for (int i = 0; i < 10; ++i) {
        a[i] = rand();
    }

    ar << a;

    std::rewind(file);

    std::map<int, int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }

    fclose(file);
}

TEST(TextFileArchive, archive_map) {
    FILE* file = std::tmpfile();
    TextFileArchive ar(file);
    std::map<int, int> a;

    for (int i = 0; i < 10; ++i) {
        a[i] = rand();
    }

    ar << a;

    std::rewind(file);

    std::map<int, int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }

    fclose(file);
}

TEST(BinaryArchive, archive_unordered_map) {
    BinaryArchive ar;
    std::unordered_map<int, int> a;

    for (int i = 0; i < 10; ++i) {
        a[i] = rand();
    }

    ar << a;
    std::unordered_map<int, int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
}

TEST(TextArchive, archive_unordered_map) {
    TextArchive ar;
    std::unordered_map<int, int> a;

    for (int i = 0; i < 10; ++i) {
        a[i] = rand();
    }

    ar << a;
    std::unordered_map<int, int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
}

TEST(BinaryFileArchive, archive_unordered_map) {
    FILE* file = std::tmpfile();
    BinaryFileArchive ar(file);
    std::unordered_map<int, int> a;

    for (int i = 0; i < 10; ++i) {
        a[i] = rand();
    }

    ar << a;

    std::rewind(file);

    std::unordered_map<int, int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }

    fclose(file);
}

TEST(TextFileArchive, archive_unordered_map) {
    FILE* file = std::tmpfile();
    TextFileArchive ar(file);
    std::unordered_map<int, int> a;

    for (int i = 0; i < 10; ++i) {
        a[i] = rand();
    }

    ar << a;

    std::rewind(file);

    std::unordered_map<int, int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }

    fclose(file);
}

TEST(BinaryArchive, archive_unordered_set) {
    BinaryArchive ar;
    std::unordered_set<int> a;

    for (int i = 0; i < 10; ++i) {
        a.insert(i);
    }

    ar << a;
    std::unordered_set<int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(a.find(i) != a.end());
        EXPECT_TRUE(b.find(i) != b.end());
    }
}

TEST(TextArchive, archive_unordered_set) {
    TextArchive ar;
    std::unordered_set<int> a;

    for (int i = 0; i < 10; ++i) {
        a.insert(i);
    }

    ar << a;
    std::unordered_set<int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(a.find(i) != a.end());
        EXPECT_TRUE(b.find(i) != b.end());
    }
}

TEST(BinaryFileArchive, archive_unordered_set) {
    FILE* file = std::tmpfile();
    BinaryFileArchive ar(file);
    std::unordered_set<int> a;

    for (int i = 0; i < 10; ++i) {
        a.insert(i);
    }

    ar << a;

    std::rewind(file);

    std::unordered_set<int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(a.find(i) != a.end());
        EXPECT_TRUE(b.find(i) != b.end());
    }

    fclose(file);
}

TEST(TextFileArchive, archive_unordered_set) {
    FILE* file = std::tmpfile();
    TextFileArchive ar(file);
    std::unordered_set<int> a;

    for (int i = 0; i < 10; ++i) {
        a.insert(i);
    }

    ar << a;

    std::rewind(file);

    std::unordered_set<int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(a.find(i) != a.end());
        EXPECT_TRUE(b.find(i) != b.end());
    }

    fclose(file);
}

TEST(BinaryArchive, archive_set) {
    BinaryArchive ar;
    std::set<int> a;

    for (int i = 0; i < 10; ++i) {
        a.insert(i);
    }

    ar << a;
    std::set<int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(a.find(i) != a.end());
        EXPECT_TRUE(b.find(i) != b.end());
    }
}

TEST(TextArchive, archive_set) {
    TextArchive ar;
    std::set<int> a;

    for (int i = 0; i < 10; ++i) {
        a.insert(i);
    }

    ar << a;
    std::set<int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(a.find(i) != a.end());
        EXPECT_TRUE(b.find(i) != b.end());
    }
}

TEST(BinaryFileArchive, archive_set) {
    FILE* file = std::tmpfile();
    BinaryFileArchive ar(file);
    std::set<int> a;

    for (int i = 0; i < 10; ++i) {
        a.insert(i);
    }

    ar << a;

    std::rewind(file);

    std::set<int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(a.find(i) != a.end());
        EXPECT_TRUE(b.find(i) != b.end());
    }

    fclose(file);
}

TEST(TextFileArchive, archive_set) {
    FILE* file = std::tmpfile();
    TextFileArchive ar(file);
    std::set<int> a;

    for (int i = 0; i < 10; ++i) {
        a.insert(i);
    }

    ar << a;

    std::rewind(file);

    std::set<int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(a.find(i) != a.end());
        EXPECT_TRUE(b.find(i) != b.end());
    }

    fclose(file);
}


TEST(BinaryArchive, archive_hash_table) {
    BinaryArchive ar;
    HashTable<int, int> a;

    for (int i = 0; i < 10; ++i) {
        a[i] = rand();
    }

    ar << a;
    HashTable<int, int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
}

TEST(TextArchive, archive_hash_table) {
    TextArchive ar;
    HashTable<int, int> a;

    for (int i = 0; i < 10; ++i) {
        a[i] = rand();
    }

    ar << a;
    HashTable<int, int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
}

TEST(BinaryFileArchive, archive_hash_table) {
    FILE* file = std::tmpfile();
    BinaryFileArchive ar(file);
    HashTable<int, int> a;

    for (int i = 0; i < 10; ++i) {
        a[i] = rand();
    }

    ar << a;

    std::rewind(file);

    HashTable<int, int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }

    fclose(file);
}

TEST(TextFileArchive, archive_hash_table) {
    FILE* file = std::tmpfile();
    TextFileArchive ar(file);
    HashTable<int, int> a;

    for (int i = 0; i < 10; ++i) {
        a[i] = rand();
    }

    ar << a;

    std::rewind(file);

    HashTable<int, int> b;
    ar >> b;
    EXPECT_EQ(a.size(), b.size());

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }

    fclose(file);
}


TEST(BinaryArchive, archive_string) {
    BinaryArchive ar;
    std::string str1 = "test";
    ar << str1;

    std::string str2;
    ar >> str2;
    EXPECT_STREQ(str1.c_str(), str2.c_str());
}

TEST(TextArchive, archive_string) {
    TextArchive ar;
    std::string str1 = "test";
    ar << str1;

    std::string str2;
    ar >> str2;
    EXPECT_STREQ(str1.c_str(), str2.c_str());
}

TEST(BinaryFileArchive, archive_string) {
    FILE* file = std::tmpfile();
    BinaryFileArchive ar(file);
    std::string str1 = "test";
    ar << str1;

    std::rewind(file);

    std::string str2;
    ar >> str2;
    EXPECT_STREQ(str1.c_str(), str2.c_str());
    fclose(file);
}

TEST(TextFileArchive, archive_string) {
    FILE* file = std::tmpfile();
    TextFileArchive ar(file);
    std::string str1 = "test";
    ar << str1;

    std::rewind(file);

    std::string str2;
    ar >> str2;
    EXPECT_STREQ(str1.c_str(), str2.c_str());
    fclose(file);
}

TEST(BinaryArchive, archive_selftype) {
    BinaryArchive ar;
    BinaryArchive sub_ar;
    std::string str1 = "test";
    sub_ar << str1;
    ar << (size_t)100 << sub_ar;

    size_t num = ar.get<size_t>();
    EXPECT_EQ(num, 100u);
    BinaryArchive br;
    ar >> br;
    std::string str2 = "";
    br >> str2;
    EXPECT_STREQ(str1.c_str(), str2.c_str());
    EXPECT_TRUE(ar.empty());
    EXPECT_TRUE(br.empty());
}

TEST(TextArchive, archive_selftype) {
    TextArchive ar;
    TextArchive sub_ar;
    std::string str1 = "test";
    sub_ar << str1;
    ar << (size_t)100 << sub_ar;

    size_t num = ar.get<size_t>();
    EXPECT_EQ(num, 100u);
    TextArchive br;
    ar >> br;
    std::string str2 = "";
    br >> str2;
    EXPECT_STREQ(str1.c_str(), str2.c_str());
    EXPECT_TRUE(ar.empty());
    EXPECT_TRUE(br.empty());
}

TEST(BinaryArchive, archive_udf_deleter) {
    BinaryArchive ar;
    BinaryArchive br;
    std::string str1 = "test";
    br << str1;
    ar.set_read_buffer(br.buffer(), br.length(), [&](void*) { br.release(); });
    std::string str2 = "";
    ar >> str2;
    ar.reset();
    EXPECT_STREQ(str1.c_str(), str2.c_str());
    EXPECT_TRUE(br.buffer() == nullptr);
}

struct MyType {
    int num = 0;
    std::string str;
    BinaryArchive ar;
    PICO_SERIALIZATION(num, str, ar);
};

TEST(BinaryArchive, archive_userdefined) {
    MyType a;
    a.num = 100;
    a.str = "test";
    a.ar << a.num << a.str;

    BinaryArchive ar;
    ar << a;

    MyType b;
    ar >> b;

    std::pair<int, std::string> p;
    b.ar >> p.first >> p.second;

    EXPECT_TRUE(ar.empty());
    EXPECT_TRUE(b.ar.empty());
    EXPECT_EQ(a.num, b.num);
    EXPECT_EQ(a.num, p.first);
    EXPECT_STREQ(a.str.c_str(), b.str.c_str());
    EXPECT_STREQ(a.str.c_str(), p.second.c_str());
}

TEST(BinaryArchive, move) {
    BinaryArchive ar;
    std::string str1 = "test";
    ar << str1;

    BinaryArchive br;
    br = std::move(ar);
    std::string str2;
    br >> str2;
    EXPECT_EQ(str1, str2);
}

TEST(TextArchive, move) {
    TextArchive ar;
    std::string str1 = "test";
    ar << str1;

    TextArchive br;
    br = std::move(ar);
    std::string str2;
    br >> str2;
    EXPECT_EQ(str1, str2);
}

TEST(BinaryFileArchive, move) {
    FILE* file = std::tmpfile();
    BinaryFileArchive ar(file);
    std::string str1 = "test";
    ar << str1;

    std::rewind(file);

    BinaryFileArchive br;
    br = std::move(ar);
    std::string str2;
    br >> str2;
    EXPECT_EQ(str1, str2);

    fclose(file);
}

TEST(TextFileArchive, move) {
    FILE* file = std::tmpfile();
    TextFileArchive ar(file);
    std::string str1 = "test";
    ar << str1;

    std::rewind(file);

    TextFileArchive br;
    br = std::move(ar);
    std::string str2;
    br >> str2;
    EXPECT_EQ(str1, str2);

    fclose(file);
}

TEST(BinaryArchive, copy) {
    BinaryArchive ar;
    std::string str1 = "test";
    int i1 = 100;
    ar << i1;
    ar << str1;
    int i2 = 0;
    ar >> i2;
    EXPECT_EQ(i1, i2);

    BinaryArchive br;
    br = ar;
    std::string str2;
    br >> str2;
    EXPECT_EQ(str1, str2);
    
    EXPECT_FALSE(ar.empty());
    EXPECT_TRUE(br.empty());
}

TEST(TextArchive, copy) {
    TextArchive ar;
    std::string str1 = "test";
    int i1 = 100;
    ar << i1;
    ar << str1;
    int i2 = 0;
    ar >> i2;
    EXPECT_EQ(i1, i2);

    TextArchive br;
    br = ar;
    std::string str2;
    br >> str2;
    EXPECT_EQ(str1, str2);
    
    EXPECT_FALSE(ar.empty());
    EXPECT_TRUE(br.empty());
}

TEST(BinaryArchive, array) {
    BinaryArchive ar;
    int a[] = {2, 3, 4, 6};
    ar << a;
    int b[4];
    ar >> b;
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
    ar.clear();
    std::string s_a[] = {"asd", "asdf", "qwer"};
    std::vector<std::string> v;
    ar << s_a;
    ar >> v;
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(s_a[i], v[i]);
    }
}

TEST(TextArchive, array) {
    TextArchive ar;
    int a[] = {2, 3, 4, 6};
    ar << a;
    int b[4];
    ar >> b;
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
    ar.clear();
    std::string s_a[] = {"asd", "asdf", "qwer"};
    std::string s_b[3];
    ar << s_a;
    ar >> s_b;
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(s_a[i], s_b[i]);
    }
}

TEST(BinaryArchive, array_string) {
    BinaryArchive ar;
    std::string a[3] = {"ab", "12", "cd"};
    ar << a;
    std::string b[3];
    ar >> b;
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(a[i], b[i]);
    }
}

} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

