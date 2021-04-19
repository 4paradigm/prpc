#ifndef PARADIGM4_PICO_PICO_CORE_SERIALIZATION_AVROHELPER_H
#define PARADIGM4_PICO_PICO_CORE_SERIALIZATION_AVROHELPER_H

#include <pico-core/common.h>
#include <pico-core/Archive.h>
#include <pico-core/StringUtility.h>
#include <avro/Compiler.hh>
#include <avro/Decoder.hh>
#include <avro/Encoder.hh>
#include <avro/Specific.hh>
#include <avro/ValidSchema.hh>
#include <boost/algorithm/string/replace.hpp>
#include <boost/any.hpp>
#include <deque>
#include <list>
#include <sstream>
#include <valarray>
#include <vector>

namespace paradigm4 {
namespace pico {
namespace core {

template <typename T, typename Enable = void>
struct avro_traits;

class AvroSchemaBuilder {
public:
    AvroSchemaBuilder(const std::string& name) : _name(name) {}

    template <typename T>
    bool add_field(const std::string& name) {
        auto ftype = avro_traits<T>::json_schema();
        _fields += "{\"type\":" + ftype + ", " + "\"name\":\"" + name + "\"},";
        return true;
    }

    std::string str() {
        std::string fields = _fields;
        if (fields.size() > 0) {
            fields[fields.size() - 1] = 0;
        }
        std::string ret =
            core::format_string("{\"fields\": [%s], \"type\": \"%s\", \"name\": \"%s\" }",
                fields.data(), "record", _name.data());
        return ret;
    }

    std::string _fields;
    std::string _name;
};

template <typename T>
std::string avro_name() {
    auto name = core::demangle(typeid(T).name());
    boost::replace_all(name, " ", "");
    boost::replace_all(name, "<", "_0");
    boost::replace_all(name, ">", "_1");
    boost::replace_all(name, ",", "_2");
    boost::replace_all(name, "paradigm4::pico::", "");
    boost::replace_all(name, "::", ".");
    return name;
}

struct avro_serial_helper {
    template <class T, class = typename T::template _avro_traits<T>>
    static std::true_type pico_has_inner_type__avro_traits_test(const T&);
    static std::false_type pico_has_inner_type__avro_traits_test(...);
    template <class T>
    using has_traits = decltype(pico_has_inner_type__avro_traits_test(std::declval<T>()));

    template <class T>
    using trait_type = typename T::template _avro_traits<T>;

    static inline std::map<std::string, std::string>& schemas() {
        static std::map<std::string, std::string> s;
        return s;
    }

    template <typename T>
    static inline std::string add_schema() {
        const std::string s = avro_traits<T>::json_schema();
        const std::string name = avro_name<T>();
        schemas()[name] = s;
        return s;
    }
};
template <class T,
    class = decltype(std::declval<T>().push_back(std::declval<typename T::value_type>()))>
std::true_type pico_has_member_func_push_back_test(const T&);
std::false_type pico_has_member_func_push_back_test(...);
template <class T>
using pico_has_member_func_push_back =
    decltype(pico_has_member_func_push_back_test(std::declval<T>()));

template <typename T, typename = void>
struct avro_type;

template <typename T>
struct avro_type<T, std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 4>> {
    static std::string value() {
        return "int";
    }
};

template <typename T>
struct avro_type<T, std::enable_if_t<std::is_integral<T>::value && sizeof(T) == 8>> {
    static std::string value() {
        return "long";
    }
};

template <>
struct avro_type<float> {
    static std::string value() {
        return "float";
    }
};

template <>
struct avro_type<bool> {
    static std::string value() {
        return "boolean";
    }
};

template <>
struct avro_type<double> {
    static std::string value() {
        return "double";
    }
};

template <>
struct avro_type<std::string> {
    static std::string value() {
        return "string";
    }
};

template <typename T>
struct avro_traits_base {
    static void encode(avro::Encoder& e, const T& v) {
        avro::encode(e, v);
    }

    static void decode(avro::Decoder& d, T& v) {
        avro::decode(d, v);
    }

    static std::string json_schema() {
        return "\"" + avro_type<T>::value() + "\"";
    }
};

template <typename T, typename Enable>
struct avro_traits : public avro_traits_base<T> {};
template <typename T>
struct avro_traits<T, std::enable_if_t<avro_serial_helper::has_traits<T>::value>>
    : public avro_serial_helper::trait_type<T> {};

template <typename T>
struct avro_traits<T, std::enable_if_t<std::is_integral<T>::value && (sizeof(T) < 4)>> {

    static void encode(avro::Encoder& e, const T& v) {
        e.encodeInt(static_cast<int32_t>(v));
    }

    static void decode(avro::Decoder& d, T& v) {
        v = static_cast<int32_t>(d.decodeInt());
    }

    static std::string json_schema() {
        return "\"" + std::string("int") + "\"";
    }
};
template <typename T>
struct avro_traits<T, std::enable_if_t<std::is_integral<T>::value && (sizeof(T) == 8)>> {

    static void encode(avro::Encoder& e, const T& v) {
        e.encodeLong(static_cast<int64_t>(v));
    }

    static void decode(avro::Decoder& d, T& v) {
        v = static_cast<T>(d.decodeLong());
    }

    static std::string json_schema() {
        return "\"" + std::string("long") + "\"";
    }
};
template <>
struct avro_traits<std::string> : public avro_traits_base<std::string> {};

template <typename T>
struct avro_traits<T, std::enable_if_t<pico_has_member_func_push_back<T>::value>> {
    typedef typename T::value_type value_type;

    /**
     * Encodes a given value.
     */
    static void encode(avro::Encoder& e, const T& b) {
        e.arrayStart();
        if (!b.empty()) {
            e.setItemCount(b.size());
            for (typename T::const_iterator it = b.begin(); it != b.end(); ++it) {
                e.startItem();
                avro_traits<value_type>::encode(e, *it);
            }
        }
        e.arrayEnd();
    }

    /**
     * Decodes into a given value.
     */
    static void decode(avro::Decoder& d, T& s) {
        s.clear();
        for (size_t n = d.arrayStart(); n != 0; n = d.arrayNext()) {
            for (size_t i = 0; i < n; ++i) {
                value_type t;
                avro_traits<value_type>::decode(d, t);
                s.push_back(t);
            }
        }
    }

    static std::string json_schema() {
        return "{\"type\": \"array\", \"items\": " +
               avro_traits<value_type>::json_schema() + "}";
    }
};
template <typename value_type>
struct avro_traits<std::valarray<value_type>> {
    typedef std::valarray<value_type> T;

    /**
     * Encodes a given value.
     */
    static void encode(avro::Encoder& e, const T& b) {
        std::vector<value_type> v(b.size());
        std::copy(std::begin(b), std::end(b), std::begin(v));
        avro_traits<std::vector<value_type>>::encode(e, v);
    }

    /**
     * Decodes into a given value.
     */
    static void decode(avro::Decoder& d, T& s) {
        std::vector<value_type> v;
        avro_traits<std::vector<value_type>>::decode(d, v);
        s.resize(v.size());
        std::copy(std::begin(v), std::end(v), std::begin(s));
    }

    static std::string json_schema() {
        return "{\"type\": \"array\", \"items\": " +
               avro_traits<value_type>::json_schema() + "}";
    }
};
template <typename value_type, size_t N>
struct avro_traits<value_type[N]> {
    using T = value_type[N];
    static void encode(avro::Encoder& e, const T& b) {
        std::vector<value_type> v(N);
        std::copy(std::begin(b), std::end(b), std::begin(v));
        avro_traits<std::vector<value_type>>::encode(e, v);
    }
    static void decode(avro::Decoder& d, T& s) {
        std::vector<value_type> v;
        avro_traits<std::vector<value_type>>::decode(d, v);
        std::copy(std::begin(v), std::end(v), std::begin(s));
    }
    static std::string json_schema() {
        return "{\"type\": \"array\", \"items\": " +
               avro_traits<value_type>::json_schema() + "}";
    }
};
template <typename value_type, size_t N>
struct avro_traits<std::array<value_type, N>> {
    using T = std::array<value_type, N>;
    static void encode(avro::Encoder& e, const T& b) {
        std::vector<value_type> v(N);
        std::copy(std::begin(b), std::end(b), std::begin(v));
        avro_traits<std::vector<value_type>>::encode(e, v);
    }
    static void decode(avro::Decoder& d, T& s) {
        std::vector<value_type> v;
        avro_traits<std::vector<value_type>>::decode(d, v);
        std::copy(std::begin(v), std::end(v), std::begin(s));
    }
    static std::string json_schema() {
        return "{\"type\": \"array\", \"items\": " +
               avro_traits<value_type>::json_schema() + "}";
    }
};

template <typename First, typename Second>
struct avro_traits<std::pair<First, Second>> {
    using T = std::pair<First, Second>;
    static void encode(avro::Encoder& e, const T& b) {
        avro_traits<First>::encode(e, b.first);
        avro_traits<Second>::encode(e, b.second);
    }
    static void decode(avro::Decoder& d, T& s) {
        avro_traits<First>::decode(d, s.first);
        avro_traits<Second>::decode(d, s.second);
    }
    static std::string json_schema() {
        AvroSchemaBuilder builder(avro_name<T>());
        builder.add_field<First>("first");
        builder.add_field<Second>("second");
        return builder.str();
    }
};

inline void _avro_encode(avro::Encoder&) {}

template <typename First, typename... _Elements>
void _avro_encode(avro::Encoder& e, First&& first, _Elements&&... args) {
    avro_traits<std::remove_cv_t<First>>::encode(e, first);
    _avro_encode(e, std::forward<_Elements>(args)...);
}

inline void _avro_decode(avro::Decoder&) {}

template <typename First, typename... _Elements>
void _avro_decode(avro::Decoder& d, First& first, _Elements&... args) {
    avro_traits<std::remove_cv_t<First>>::decode(d, first);
    _avro_decode(d, args...);
}

template <typename... _Elements>
struct avro_traits<std::tuple<_Elements...>> {
    using T               = std::tuple<_Elements...>;
    static const size_t N = std::tuple_size<T>::value;

    static void encode(avro::Encoder& e, const T& b) {
        _encode(e, b, std::make_index_sequence<N>());
    }
    static void decode(avro::Decoder& d, T& s) {
        _decode(d, s, std::make_index_sequence<N>());
    }

    static std::string json_schema() {
        AvroSchemaBuilder builder(avro_name<T>());
        _add_field(builder, std::make_index_sequence<N>());
        return builder.str();
    }

private:
    template <size_t... I>
    static void _encode(avro::Encoder& e, const T& b, std::index_sequence<I...>) {
        _avro_encode(e, std::get<I>(b)...);
    }
    template <size_t... I>
    static void _decode(avro::Decoder& d, T& s, std::index_sequence<I...>) {
        _avro_decode(d, std::get<I>(s)...);
    }

    template <size_t... I>
    static void _add_field(AvroSchemaBuilder& builder, std::index_sequence<I...>) {
        auto unused = {
            builder.add_field<std::tuple_element_t<I, T>>("_" + std::to_string(I))...};
        (void)unused;
    }
};


}
} // namespace pico
} // namespace paradigm4

// avro helper
namespace avro {




template <class T>
struct avro_schema {};

} // namespace avro

#endif
