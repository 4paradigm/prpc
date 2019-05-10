#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <sstream>

#include <glog/logging.h>
#include <gtest/gtest.h>
#include "common.h"
#include "PicoJsonNode.h"

namespace paradigm4 {
namespace pico {
namespace core {

TEST(PicoJsonNodeTest, pico_json_node_constructor_check)
{
    // null
    PicoJsonNode null1;
    PicoJsonNode null2(nullptr);
    PicoJsonNode null3 = {};
    EXPECT_TRUE(null1 == null2);
    EXPECT_TRUE(null2 == null3);

    // object
    std::map<std::string, std::string> m = {{"key", "value"}};
    PicoJsonNode object1({{"key", "value"}});
    PicoJsonNode object2(object1);
    PicoJsonNode object3(m);
    PicoJsonNode object4 = {{"key", "value"}};
    PicoJsonNode object5 = m;
    EXPECT_TRUE(object1 == object2);
    EXPECT_TRUE(object2 == object3);
    EXPECT_TRUE(object3 == object4);
    EXPECT_TRUE(object4 == object5);

    m.clear();
    PicoJsonNode null_object1({});
    PicoJsonNode null_object2 = PicoJsonNode::object();
    PicoJsonNode null_object3 = m;
    EXPECT_TRUE(null_object1 == null_object2);
    EXPECT_TRUE(null_object2 == null_object3);
    EXPECT_TRUE(null_object1 != null1);

    // array
    std::vector<int> v = {1, 2, 3, 4};
    PicoJsonNode array1({1, 2, 3, 4});
    PicoJsonNode array2(array1);
    PicoJsonNode array3(v);
    PicoJsonNode array4 = {1, 2, 3, 4};
    PicoJsonNode array5 = v;
    EXPECT_TRUE(array1 == array2);
    EXPECT_TRUE(array2 == array3);
    EXPECT_TRUE(array3 == array4);
    EXPECT_TRUE(array4 == array5);

    v.clear();
    PicoJsonNode null_array1 = PicoJsonNode::array();
    PicoJsonNode null_array2 = v;
    EXPECT_TRUE(null_array1 == null_array2);
    EXPECT_TRUE(null_array1 != null1);

    // string
    std::string s = "test";
    PicoJsonNode string1("test");
    PicoJsonNode string2(string1);
    PicoJsonNode string3(s);
    PicoJsonNode string4 = "test";
    PicoJsonNode string5 = s;
    EXPECT_TRUE(string1 == string2);
    EXPECT_TRUE(string2 == string3);
    EXPECT_TRUE(string3 == string4);
    EXPECT_TRUE(string4 == string5);

    PicoJsonNode null_string = "";

    // boolean
    PicoJsonNode bool1(false);
    PicoJsonNode bool2(bool1);
    PicoJsonNode bool3 = false;
    EXPECT_TRUE(bool1 == bool2);
    EXPECT_TRUE(bool2 == bool3);

    // int
    PicoJsonNode int1(0);
    PicoJsonNode int2(int1);
    PicoJsonNode int3 = 0;
    EXPECT_TRUE(int1 == int2);
    EXPECT_TRUE(int2 == int3);
    EXPECT_TRUE(int1 != bool1);

    // float
    PicoJsonNode float1(0.0);
    PicoJsonNode float2(float1);
    PicoJsonNode float3 = 0.0;
    EXPECT_TRUE(float1 == float2);
    EXPECT_TRUE(float2 == float3);
    EXPECT_TRUE(float1 == int1);

    // construct from type
    PicoJsonNode type_null(PicoJsonNode::value_t::null);
    PicoJsonNode type_object(PicoJsonNode::value_t::object);
    PicoJsonNode type_array(PicoJsonNode::value_t::array);
    PicoJsonNode type_string(PicoJsonNode::value_t::string);
    PicoJsonNode type_boolean(PicoJsonNode::value_t::boolean);
    PicoJsonNode type_number_integer(PicoJsonNode::value_t::number_integer);
    PicoJsonNode type_number_unsigned(PicoJsonNode::value_t::number_unsigned);
    PicoJsonNode type_number_float(PicoJsonNode::value_t::number_float);
    EXPECT_TRUE(type_null == null1);
    EXPECT_TRUE(type_object == null_object1);
    EXPECT_TRUE(type_array == null_array1);
    EXPECT_TRUE(type_string == null_string);
    EXPECT_TRUE(type_boolean == bool1);
    EXPECT_TRUE(type_number_integer == int1);
    EXPECT_TRUE(type_number_unsigned == int1);
    EXPECT_TRUE(type_number_float == float1);
}

TEST(PicoJsonNodeTest, pico_json_node_load_and_save_check)
{
    std::string complex_object = "{\"key1\":\"value1\",\"key2\":[1,2,3,4],\"key3\":1,\"key4\":true,\"key5\":null,\"key6\":1.123,\"key7\":{\"inner_key\":\"inner_value\"}}";
    std::stringstream ss;
    FILE* fid = std::tmpfile();

    ss << complex_object;
    fprintf(fid, "%s", complex_object.c_str());
    std::rewind(fid);

    // load
    PicoJsonNode file_in1;
    PicoJsonNode file_in2;
    PicoJsonNode file_in3;
    PicoJsonNode file_in4;
    file_in1.load(complex_object);
    file_in2.load(ss);
    file_in3.load(fid);
    EXPECT_TRUE(file_in1 == file_in2);
    EXPECT_TRUE(file_in2 == file_in3);

    // save
    std::string save_to;
    ss.clear();
    std::rewind(fid);

    file_in1.save(save_to);
    file_in1.save(ss);
    file_in1.save(fid);

    std::rewind(fid);
    file_in2.load(save_to);
    file_in3.load(ss);
    file_in4.load(fid);
    EXPECT_TRUE(file_in1 == file_in2);
    EXPECT_TRUE(file_in2 == file_in3);
    EXPECT_TRUE(file_in3 == file_in4);
}

TEST(PicoJsonNodeTest, pico_json_node_instance_type_check)
{
    PicoJsonNode type_null(PicoJsonNode::value_t::null);
    PicoJsonNode type_object(PicoJsonNode::value_t::object);
    PicoJsonNode type_array(PicoJsonNode::value_t::array);
    PicoJsonNode type_string(PicoJsonNode::value_t::string);
    PicoJsonNode type_boolean(PicoJsonNode::value_t::boolean);
    PicoJsonNode type_number_integer(PicoJsonNode::value_t::number_integer);
    PicoJsonNode type_number_unsigned(PicoJsonNode::value_t::number_unsigned);
    PicoJsonNode type_number_float(PicoJsonNode::value_t::number_float);
   
    EXPECT_TRUE(type_null.is_null());
    EXPECT_TRUE(type_object.is_object());
    EXPECT_TRUE(type_array.is_array());
    EXPECT_TRUE(type_string.is_string());
    EXPECT_TRUE(type_boolean.is_boolean());
    EXPECT_TRUE(type_number_integer.is_number_integer());
    EXPECT_TRUE(type_number_unsigned.is_number_unsigned());
    EXPECT_TRUE(type_number_unsigned.is_number_integer());
    EXPECT_TRUE(type_number_float.is_number_float());
    
    EXPECT_FALSE(type_null.is_object());
    EXPECT_FALSE(type_object.is_array());
    EXPECT_FALSE(type_array.is_string());
    EXPECT_FALSE(type_string.is_number_integer());
    EXPECT_FALSE(type_number_integer.is_number_float());
    EXPECT_FALSE(type_number_integer.is_number_unsigned());
    EXPECT_FALSE(type_number_unsigned.is_number_float());
    EXPECT_FALSE(type_number_float.is_boolean());
    EXPECT_FALSE(type_boolean.is_null());

    EXPECT_TRUE(type_null.is_primitive());
    EXPECT_TRUE(type_string.is_primitive());
    EXPECT_TRUE(type_number_integer.is_primitive());
    EXPECT_TRUE(type_number_unsigned.is_primitive());
    EXPECT_TRUE(type_boolean.is_primitive());
    EXPECT_TRUE(type_number_float.is_primitive());
    
    EXPECT_TRUE(type_array.is_structured());
    EXPECT_TRUE(type_object.is_structured());
    
    EXPECT_TRUE(type_number_integer.is_number());
    EXPECT_TRUE(type_number_unsigned.is_number());
    EXPECT_TRUE(type_number_float.is_number());

    EXPECT_FALSE(type_null.is_structured());
    EXPECT_FALSE(type_string.is_structured());
    EXPECT_FALSE(type_number_integer.is_structured());
    EXPECT_FALSE(type_number_unsigned.is_structured());
    EXPECT_FALSE(type_number_float.is_structured());
    EXPECT_FALSE(type_boolean.is_structured());
    EXPECT_FALSE(type_array.is_primitive());
    EXPECT_FALSE(type_object.is_primitive());
    EXPECT_FALSE(type_boolean.is_number());
    EXPECT_FALSE(type_array.is_number());
    EXPECT_FALSE(type_object.is_number());
    EXPECT_FALSE(type_null.is_number());
}

TEST(PicoJsonNodeTest, pico_json_node_value_access_check)
{
    /*  instance:
    {
        "key1": "1234",
        "key2": [1, 2, 3, 4],
        "key3": 1,
        "key4": true,
        "key5": null,
        "key6": 1.123,
        "key7": {
            "inner_key": "inner_value"
        }
    }
    */
    std::string complex_object = "{\"key1\":\"1234\",\"key2\":[1,2,3,4],\"key3\":1,\"key4\":true,\"key5\":null,\"key6\":1.123,\"key7\":{\"inner_key\":\"inner_value\"}}";
    PicoJsonNode instance;
    instance.load(complex_object);

    PicoJsonNode key1 = "1234";
    PicoJsonNode key2 = {1, 2, 3, 4};
    PicoJsonNode key3 = 1;
    PicoJsonNode key4 = true;
    PicoJsonNode key5;
    PicoJsonNode key6 = 1.123;
    PicoJsonNode key7 = {{"inner_key", "inner_value"}};

    // value access
    EXPECT_TRUE(instance.at("key1") == key1);
    EXPECT_TRUE(instance.at("key2") == key2);
    EXPECT_TRUE(instance.at("key3") == key3);
    EXPECT_TRUE(instance.at("key4") == key4);
    EXPECT_TRUE(instance.at("key5") == key5);
    EXPECT_TRUE(instance.at("key6") == key6);
    EXPECT_TRUE(instance.at("key7") == key7);
    EXPECT_TRUE(instance.at("key2", 0) == key3);

    EXPECT_TRUE(instance.at("key1").is_valid());
    EXPECT_TRUE(instance.at("key2").is_valid());
    EXPECT_TRUE(instance.at("key3").is_valid());
    EXPECT_TRUE(instance.at("key4").is_valid());
    EXPECT_TRUE(instance.at("key5").is_valid());
    EXPECT_TRUE(instance.at("key6").is_valid());
    EXPECT_TRUE(instance.at("key7").is_valid());
    EXPECT_TRUE(instance.at("key2", 0).is_valid());

    // primitive value convert
    EXPECT_TRUE(instance.at("key1").as<std::string>() == "1234");
    EXPECT_TRUE(instance.at("key3").as<int>() == 1);
    EXPECT_TRUE(instance.at("key4").as<bool>() == true);
    EXPECT_TRUE(instance.at("key2", 2).as<int>() == 3);
    EXPECT_TRUE(abs(instance.at("key6").as<float>()-1.123) < 1e-6);

    std::string value1 = "1234", tmp_value1;
    int value2, value3 = 1, tmp_value3;
    bool value4 = true, tmp_value4;
    float value6 = 1.123, tmp_value6;
    int value22 = 3, tmp_value22;
    instance.at("key1").try_as<std::string>(tmp_value1);
    instance.at("key3").try_as<int>(tmp_value3);
    instance.at("key4").try_as<bool>(tmp_value4);
    instance.at("key6").try_as<float>(tmp_value6);
    instance.at("key2", 2).try_as<int>(tmp_value22);
    EXPECT_TRUE(value1 == tmp_value1);
    EXPECT_EQ(value3,  tmp_value3);
    EXPECT_EQ(value4,  tmp_value4);
    EXPECT_EQ(value6,  tmp_value6);
    EXPECT_EQ(value22, tmp_value22);

    // key not exist
    PicoJsonNode error_node = instance.at("key1", "other_key", 0, 2333);
    EXPECT_FALSE(error_node.is_valid());

    // as with default value
    value1 = instance.at("key100").as<std::string>("defv");
    value2 = instance.at("key2").as<int>(12345);
    value3 = instance.at("key3", 0).as<int>(54321);
    EXPECT_TRUE(value1 == "defv");
    EXPECT_EQ(value2, 12345);
    EXPECT_EQ(value3, 54321);

    // has
    EXPECT_TRUE(instance.has("key1"));
    EXPECT_TRUE(instance.has("key2", 0));
    EXPECT_TRUE(instance.has("key7", "inner_key"));
    EXPECT_FALSE(instance.has("key8"));
    EXPECT_FALSE(instance.has("key1", "gg", "ff"));
}

TEST(PicoJsonNodeTest, pico_json_node_stl_like_func_check)
{
    PicoJsonNode array1 = {1, 2, 3, 4};
    EXPECT_EQ(array1.size(), 4u);
    EXPECT_FALSE(array1.empty());
    array1.clear();
    EXPECT_EQ(array1.size(), 0u);
    EXPECT_TRUE(array1.empty());

    // array iterator
    array1 = {1, 2, 4, 8, 16, 32};
    int checksum = 0;
    for(auto it = array1.begin(); it != array1.end(); it++) {
        checksum += it->as<int>();
    }
    EXPECT_EQ(checksum, 63);

    checksum = 0;
    for(auto ele: array1) {
        checksum += ele.as<int>();
    }
    EXPECT_EQ(checksum, 63);

    // object iterator
    PicoJsonNode object1 = {{"k1", 1}, {"k2", 2}, {"k3", 4}, {"k4", 8}, {"k5", 16}, {"k6", 32}};
    bool visit[10] = {0};
    checksum = 0;
    for(auto it = object1.begin(); it != object1.end(); it++) {
        int id;
        sscanf(it.key().c_str(), "k%d", &id);
        EXPECT_TRUE(1 <= id && id <= 6 && !visit[id]);
        visit[id] = true;

        checksum += it.value().as<int>();
    }
    EXPECT_EQ(checksum, 63);

    checksum = 0;
    for(auto ele: object1) {
        checksum += ele.as<int>();
    }
    EXPECT_EQ(checksum, 63);

    // primitive iterator
    PicoJsonNode int1 = 12345678;
    checksum = 0;
    for(auto it = int1.begin(); it != int1.end(); it++) {
        checksum += it->as<int>();
    }
    EXPECT_EQ(checksum, 12345678);

    checksum = 0;
    for(auto ele: int1) {
        checksum += ele.as<int>();
    }
    EXPECT_EQ(checksum, 12345678);

    PicoJsonNode primitive;
    for(auto it = primitive.begin(); it != primitive.end(); ++it) {
        EXPECT_TRUE(it.key() == "");
    }
}

TEST(PicoJsonNodeTest, pico_json_node_add_erase_modify_check)
{
    // array 
    PicoJsonNode array1 = {0, 1, 2, 3, 4, 5};
    PicoJsonNode array2 = {0, 1, 2, 3, 4, 5, 6};
    PicoJsonNode array3 = {0, 1, 2, 4, 5, 6};
    PicoJsonNode array4 = {7, 1, 2, 4, 5, 6};
    EXPECT_TRUE(array1.push_back(6));
    EXPECT_TRUE(array1 == array2);
    EXPECT_TRUE(array1.erase(3));
    EXPECT_TRUE(array1 == array3);
    EXPECT_TRUE(array1.modify(0, 7));
    EXPECT_TRUE(array1 == array4);

    EXPECT_FALSE(array1.erase(20));
    EXPECT_TRUE(array1 == array4);
    EXPECT_FALSE(array1.erase(-1));
    EXPECT_TRUE(array1 == array4);
    EXPECT_FALSE(array1.modify(20, 20));
    EXPECT_TRUE(array1 == array4);
    EXPECT_FALSE(array1.modify(-1, 20));
    EXPECT_TRUE(array1 == array4);
    EXPECT_FALSE(array1.modify(0, 1, 2, 20));
    EXPECT_TRUE(array1 == array4);

    EXPECT_FALSE(array1.add("key", "value"));  // array type only allow to use push_back
    EXPECT_TRUE(array1 == array4);

    // object
    PicoJsonNode object1;
    PicoJsonNode object2 = {{"k1", "v1"}, {"k2", "v2"}};
    PicoJsonNode object3 = {{"k2", "v2"}};
    PicoJsonNode object4 = {{"k2", "v3"}};

    EXPECT_TRUE(object1.add("k1", "v1"));
    EXPECT_TRUE(object1.add("k2", "v2"));
    EXPECT_TRUE(object1 == object2);
    EXPECT_TRUE(object1.erase("k1"));
    EXPECT_TRUE(object1 == object3);
    EXPECT_TRUE(object1.modify("k2", "v3"));
    EXPECT_TRUE(object1 == object4);

    std::cout << object1 << std::endl;
    EXPECT_FALSE(object1.erase("k3"));
    EXPECT_TRUE(object1 == object4);
    EXPECT_FALSE(object1.modify("k3", "v4"));
    EXPECT_TRUE(object1 == object4);
    EXPECT_FALSE(object1.modify("k1", 0, "233", "v4"));
    EXPECT_TRUE(object1 == object4);
    
    EXPECT_FALSE(object1.push_back("v4"));
    EXPECT_TRUE(object1 == object4);

}

} // namespace core
} // namespace pico
} // namespace paradigm4

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
