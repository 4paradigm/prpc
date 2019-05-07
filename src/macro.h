#ifndef PARADIGM4_PICO_COMMON_MACRO_H
#define PARADIGM4_PICO_COMMON_MACRO_H

#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/facilities/overload.hpp>
#include "pico_log.h"

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



#ifndef PICO_TRY_CATCH
#define PICO_TRY_CATCH(expr) \
    try { \
        expr; \
    } catch (paradigm4::pico::PicoException& e) { \
        SLOG(FATAL) << "PicoException: " << e.what(); \
    } catch (std::exception& e) { \
        SLOG(FATAL) << "std::exception: " << e.what(); \
    }
#endif // PICO_TRY_CATCH

#ifndef PICO_FILE_LINENUM
#define PICO_FILE_LINENUM \
    (boost::format("%s_%d") % __FILE__ % __LINE__).str()
#endif // PICO_FILE_LINENUM

#ifndef PICO_MAIN
#define PICO_MAIN(inner_main) \
    int main(int argc, char* argv[]) { \
        int ret = 0; \
        try { \
            paradigm4::pico::pico_initialize(argc, argv); \
            ret = inner_main(argc, argv); \
            paradigm4::pico::pico_finalize(); \
        } catch (paradigm4::pico::PicoException& e) { \
            SLOG(FATAL) << "PicoException: " << e.what(); \
        } catch (std::exception& e) { \
            SLOG(FATAL) << "STL exception: " << e.what(); \
        } catch (...) { \
            SLOG(FATAL) << "unknown exception"; \
        } \
        return ret; \
    }
#endif // PICO_MAIN

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

#ifndef PICO_APP_CONFIG
#define PICO_APP_CONFIG(name, type, deftval, desc, is_missing_ok) \
    static type G_ ## name = deftval; \
    static auto s_inner_checker_G_ ## name = paradigm4::pico::DefaultChecker<type>(); \
    static auto s_inner_checker_func_G_ ## name = s_inner_checker_G_ ## name.checker(); \
    static std::string s_inner_checker_string_G_ ## name = s_inner_checker_G_ ## name.tostring(); \
    static bool s_g_is_missing_ok_G_ ## name = is_missing_ok; \
    static paradigm4::pico::ConfigureLoaderRegister s_g_config_loader_reg_G_ ## name ( \
            inner_app_config_helper(), {#name, \
            #type, \
            desc, \
            paradigm4::pico::pico_lexical_cast<std::string>(G_ ## name), \
            s_g_is_missing_ok_G_ ## name, \
            []() -> bool { \
                if (s_g_is_missing_ok_G_ ## name) { \
                    G_ ## name = paradigm4::pico::app_config().get<type>(#name, G_ ## name); \
                    return true; \
                } else { \
                    return paradigm4::pico::app_config()[#name].try_as(G_ ## name); \
                } \
            }, \
            []() -> std::string { \
                return paradigm4::pico::pico_lexical_cast<std::string>(G_ ## name); \
            }, \
            []() -> bool {return s_inner_checker_func_G_ ## name(G_ ## name, #name);}, \
            s_inner_checker_string_G_ ## name})
#endif // PICO_APP_CONFIG

#ifndef PICO_FRAMEWORK_CONFIG
#define PICO_FRAMEWORK_CONFIG(name, type, deftval, desc, is_missing_ok) \
    static type P_ ## name = deftval; \
    static auto s_inner_checker_P_ ## name = paradigm4::pico::DefaultChecker<type>(); \
    static auto s_inner_checker_func_P_ ## name = s_inner_checker_P_ ## name.checker(); \
    static std::string s_inner_checker_string_P_ ## name = s_inner_checker_P_ ## name.tostring(); \
    static bool s_g_is_missing_ok_P_ ## name = is_missing_ok; \
    static paradigm4::pico::ConfigureLoaderRegister s_g_config_loader_reg_P_ ## name ( \
            inner_framework_config_helper(), {#name, \
            #type, \
            desc, \
            paradigm4::pico::pico_lexical_cast<std::string>(P_ ## name), \
            s_g_is_missing_ok_P_ ## name, \
            []() -> bool { \
                if (s_g_is_missing_ok_P_ ## name) { \
                    P_ ## name = paradigm4::pico::framework_config().get<type>(#name, P_ ## name); \
                    return true; \
                } else { \
                    return paradigm4::pico::framework_config()[#name].try_as(P_ ## name); \
                } \
            }, \
            []() -> std::string { \
                return paradigm4::pico::pico_lexical_cast<std::string>(P_ ## name); \
            }, \
            []() -> bool {return s_inner_checker_func_P_ ## name(P_ ## name, #name);}, \
            s_inner_checker_string_P_ ## name})
#endif // PICO_FRAMEWORK_CONFIG

#ifndef PICO_APP_CONFIG_CHK
#define PICO_APP_CONFIG_CHK(name, type, deftval, desc, is_missing_ok, cfg_checker) \
    static type G_ ## name = deftval; \
    static auto s_inner_checker_G_ ## name = cfg_checker; \
    static auto s_inner_checker_func_G_ ## name = s_inner_checker_G_ ## name.checker(); \
    static std::string s_inner_checker_string_G_ ## name = s_inner_checker_G_ ## name.tostring(); \
    static bool s_g_is_missing_ok_G_ ## name = is_missing_ok; \
    static paradigm4::pico::ConfigureLoaderRegister s_g_config_loader_reg_G_ ## name ( \
            inner_app_config_helper(), {#name, \
            #type, \
            desc, \
            paradigm4::pico::pico_lexical_cast<std::string>(G_ ## name), \
            s_g_is_missing_ok_G_ ## name, \
            []() -> bool { \
                if (s_g_is_missing_ok_G_ ## name) { \
                    G_ ## name = paradigm4::pico::app_config().get<type>(#name, G_ ## name); \
                    return true; \
                } else { \
                    return paradigm4::pico::app_config()[#name].try_as(G_ ## name); \
                } \
            }, \
            []() -> std::string { \
                return paradigm4::pico::pico_lexical_cast<std::string>(G_ ## name); \
            }, \
            []() -> bool {return s_inner_checker_func_G_ ## name(G_ ## name, #name);}, \
            s_inner_checker_string_G_ ## name})
#endif // PICO_APP_CONFIG_CHK

#ifndef PICO_FRAMEWORK_CONFIG_CHK
#define PICO_FRAMEWORK_CONFIG_CHK(name, type, deftval, desc, is_missing_ok, cfg_checker) \
    static type P_ ## name = deftval; \
    static auto s_inner_checker_P_ ## name = cfg_checker; \
    static auto s_inner_checker_func_P_ ## name = s_inner_checker_P_ ## name.checker(); \
    static std::string s_inner_checker_string_P_ ## name = s_inner_checker_P_ ## name.tostring(); \
    static bool s_g_is_missing_ok_P_ ## name = is_missing_ok; \
    static paradigm4::pico::ConfigureLoaderRegister s_g_config_loader_reg_P_ ## name ( \
            inner_framework_config_helper(), {#name, \
            #type, \
            desc, \
            paradigm4::pico::pico_lexical_cast<std::string>(P_ ## name), \
            s_g_is_missing_ok_P_ ## name, \
            []() -> bool { \
                if (s_g_is_missing_ok_P_ ## name) { \
                    P_ ## name = paradigm4::pico::framework_config().get<type>(#name, P_ ## name); \
                    return true; \
                } else { \
                    return paradigm4::pico::framework_config()[#name].try_as(P_ ## name); \
                } \
            }, \
            []() -> std::string { \
                return paradigm4::pico::pico_lexical_cast<std::string>(P_ ## name); \
            }, \
            []() -> bool {return s_inner_checker_func_P_ ## name(P_ ## name, #name);}, \
            s_inner_checker_string_P_ ## name})
#endif // PICO_FRAMEWORK_CONFIG_CHK


#ifndef PICO_MEMBER_TYPE_ASSERT
#define PICO_MEMBER_TYPE_ASSERT(classname, member, type) \
    static_assert(std::is_same<decltype(std::declval<classname>().member), type>::value, \
            #classname"."#member"'s type must be "#type)
#endif // PICO_MEMBER_TYPE_ASSERT


/*
 *  do not allow multiple definition
 *  of factories with same factory_name.
 */
#ifndef PICO_DEFINE_FACTORY
#define PICO_DEFINE_FACTORY(factory_name, ArgTypes...) \
    namespace pico_factory { \
    class pico_##factory_name##_factory: public paradigm4::pico::Factory<ArgTypes> { \
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

#endif // PARADIGM4_PICO_COMMON_MACRO_H
