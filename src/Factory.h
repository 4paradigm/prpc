#ifndef PARADIGM4_PICO_COMMON_FACTORY_H
#define PARADIGM4_PICO_COMMON_FACTORY_H

#include <unordered_map>
#include <typeinfo>
#include <string>
#include <functional>
#include <memory>

#include "VirtualObject.h"

namespace paradigm4 {
namespace pico {
namespace core {

/*!
 * \brief Factory class include a string to function map
 */
template <typename... ArgTypes>
class Factory : public VirtualObject {
public:
    /*!
     * \brief get function of name $name
     */

    void* create(const std::string& name, ArgTypes... args) const {
        auto it = _producers.find(name);
        if (it == _producers.end()) {
            return nullptr;
        } else {
            return (it->second)(std::forward<ArgTypes>(args)...);
        }
    }

    /*!
     * \brief bind typename of T to a fuction return new(std::nothrow) T
     */
    template<class T>
    std::shared_ptr<T> make_shared(const std::string& name, ArgTypes... args) const { // WARNING: it's dangerous!!
        return std::shared_ptr<T>(static_cast<T*>(create(name, std::forward<ArgTypes>(args)...)));
    }

    template<class T>
    bool register_producer() {
        return register_producer<T>(typeid(T).name());
    }

    /*!
     * \brief bind $name to a fuction return new(std::nothrow) T
     */
    template<class T>
    bool register_producer(const std::string& name) { // user defined type name
        auto it = _producers.find(name);
        if (it == _producers.end()) {
            _producers[name] = [](ArgTypes... args) -> void* {
                return new(std::nothrow) T(std::forward<ArgTypes>(args)...);
            };
            return true;
        }
        return false;
    }

    template<class T>
    std::string get_type_name() const {
        return typeid(T).name();
    }

    /*!
     * \brief check $name is in map
     */
    bool exist(const std::string& name) const {
        auto it = _producers.find(name);
        return it != _producers.end();
    }

    /*!
     * \brief check typename T is in map
     */
    template<class T>
    bool exist() const {
        return exist(typeid(T).name());
    }

private:
    std::unordered_map<std::string, std::function<void*(ArgTypes...)>> _producers;
};

} // namespace core
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_COMMON_FACTORY_H
