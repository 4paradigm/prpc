#ifndef PARADIGM4_PICO_CORE_MACRO_H
#define PARADIGM4_PICO_CORE_MACRO_H

#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/facilities/overload.hpp>

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif // likely

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif // unlikely

#ifndef PICO_DEPRECATED
#define PICO_DEPRECATED(msg) [[deprecated(msg)]]
#endif // PICO_DEPRECATED

#define PICO_UNUSED __attribute__((unused))
#define PICO_DONT_INLINE __attribute__((noinline))

#define NARGS(...) __NARGS(0, ## __VA_ARGS__, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10,\
                                               9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define __NARGS(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, \
                _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, N, ...) N
#define PICO_PP_OVERLOAD(prefix, ...) BOOST_PP_CAT(prefix, NARGS(__VA_ARGS__))
#define PICO_CAT BOOST_PP_CAT
#define MACRO_OVERLOAD_0(m, i, a)
#define MACRO_OVERLOAD_1(m, i, a) m(a,i)
#define MACRO_OVERLOAD_2(m, i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_1(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_3(m, i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_2(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_4(m, i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_3(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_5(m, i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_4(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_6(m, i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_5(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_7(m, i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_6(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_8(m, i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_7(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_9(m, i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_8(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_10(m,i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_9(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_11(m,i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_10(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_12(m,i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_11(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_13(m,i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_12(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_14(m,i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_13(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_15(m,i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_14(m, BOOST_PP_INC(i), other)
#define MACRO_OVERLOAD_16(m,i, a, other...) MACRO_OVERLOAD_1(m, i, a) MACRO_OVERLOAD_15(m, BOOST_PP_INC(i), other)
/*
 *
 * #define macro(a,i) a=i;
 * PICO_FOREACH(macro,a,b,c,d)
 * expand to:
 * a=0;
 * b=1;
 * c=2;
 * d=3;
 *
 */
#define PICO_FOREACH(m,...) \
    PICO_PP_OVERLOAD(MACRO_OVERLOAD_, __VA_ARGS__)(m, 0, __VA_ARGS__)


/*
 *  do not allow multiple definition
 *  of factories with same factory_name.
 */
#ifndef PICO_DEFINE_FACTORY
#define PICO_DEFINE_FACTORY(factory_name, ArgTypes...) \
    namespace pico_factory { \
    class pico_##factory_name##_factory: public paradigm4::pico::core::Factory<ArgTypes> { \
        public: \
            static pico_##factory_name##_factory & singleton() { \
                static pico_##factory_name##_factory factory; \
                return factory; \
            } \
        private: \
            pico_##factory_name##_factory() = default;\
    }; \
    template<class T> \
    class pico_##factory_name##_register_agent { \
        public:\
        pico_##factory_name##_register_agent(const std::string& key) { \
            pico_##factory_name##_factory::singleton().register_producer<T>(key); \
        } \
    }; \
    } \
    template<class T, typename... Args> \
    T* pico_##factory_name##_create(const std::string& key, Args&&... args) { \
        return static_cast<T*>(pico_factory::pico_##factory_name##_factory::singleton().create(key, std::forward<Args>(args)...));\
    }\
    template<class T, typename... Args> \
    std::shared_ptr<T> pico_##factory_name##_make_shared(const std::string& key, Args&&... args) { \
        return pico_factory::pico_##factory_name##_factory::singleton().make_shared<T>(key, std::forward<Args>(args)...); \
    } \
 
#endif // PICO_DEFINE_FACTORY


/*
 *  Strongly recommend that reflect class is
 *  subclass of paradigm4::pico::Object, to
 *  avoid memory leak
 */ 
#ifndef PICO_FACTORY_REGISTER
#define PICO_FACTORY_REGISTER(factory_name, class_name, key) \
    namespace pico_factory { \
    typedef class_name factory_name##key;\
    static pico_##factory_name##_register_agent< factory_name##key > factory_name##_##key##_agent\
        = pico_##factory_name##_register_agent< factory_name##key >(#key);\
    }
#endif // PICO_FACTORY_REGISTER

#ifndef PICO_FACTORY_REGISTER2
#define PICO_FACTORY_REGISTER2(factory_name, class_name, key) \
    namespace pico_factory{ \
    static pico_##factory_name##_register_agent< class_name > factory_name_##class_name##_agent\
        = pico_##factory_name##_register_agent< class_name >(key);\
    }
#endif // PICO_FACTORY_REGISTER


#ifndef PICO_DEFINE_MEMBER_CHECKER
#define PICO_DEFINE_MEMBER_CHECKER(member) \
    template<class T, class = decltype(std::declval<T>().member)> \
    std::true_type pico_has_member_ ## member ## _test(const T&); \
    std::false_type pico_has_member_ ## member ## _test(...); \
    template<class T> using pico_has_member_ ## member = \
        decltype(pico_has_member_ ## member ## _test(std::declval<T>()))
#endif // PICO_DEFINE_MEMBER_CHECKER

#ifndef PICO_DEFINE_MEMBER_FUNC_CHECKER
#define PICO_DEFINE_MEMBER_FUNC_CHECKER(member) \
    template<class T, class = decltype(std::declval<T>().member())> \
    std::true_type pico_has_member_func_ ## member ## _test(const T&); \
    std::false_type pico_has_member_func_ ## member ## _test(...); \
    template<class T> using pico_has_member_func_ ## member = \
        decltype(pico_has_member_func_ ## member ## _test(std::declval<T>()))
#endif // PICO_DEFINE_MEMBER_FUNC_CHECKER

#ifndef PICO_DEFINE_INNER_TYPE_CHECKER
#define PICO_DEFINE_INNER_TYPE_CHECKER(member) \
    template<class T, class = typename T::member> \
    std::true_type pico_has_inner_type_ ## member ## _test(const T&); \
    std::false_type pico_has_inner_type_ ## member ## _test(...); \
    template<class T> using pico_has_inner_type_ ## member = \
        decltype(pico_has_inner_type_ ## member ## _test(std::declval<T>()))
#endif // PICO_DEFINE_INNER_TYPE_CHECKER

#endif // PARADIGM4_PICO_CORE_MACRO_H
