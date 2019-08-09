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


#endif // PARADIGM4_PICO_CORE_MACRO_H
