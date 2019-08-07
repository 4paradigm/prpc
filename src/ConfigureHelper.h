#ifndef PARADIGM4_PICO_COMMON_CONFIGURE_HELPER_H
#define PARADIGM4_PICO_COMMON_CONFIGURE_HELPER_H

#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "Configure.h"
#include "pico_log.h"

namespace paradigm4 {
namespace pico {
namespace core {

struct ConfigUnit {
    std::string name;
    std::string type;
    std::string desc;
    std::string defv;
    bool is_missing_ok;
    bool is_structure;
    std::string checker_info;

    ConfigUnit() {}

    ConfigUnit(const std::string& n, const std::string& t, const std::string& ds,
               const std::string& dv, bool imo, bool is, const std::string& ci) {
        name = n;
        type = t;
        desc = ds;
        defv = dv;
        is_missing_ok = imo;
        is_structure = is;
        checker_info = ci;
    }

    std::string helper_info() const {
        std::string ret;
        if (is_missing_ok) {
            ret = "Optional: " + name + " = " + defv + ", " + type + ", " + desc +
                  ", constraint: " + checker_info;
        } else {
            ret = "Required: " + name + ", " + type + ", " + desc +
                  ", constraint: " + checker_info;
        }
        return ret;
    }
};

class CustomConfigNode {
public:
    virtual void init_config() = 0;

    virtual bool load_config(const Configure&) = 0;

    virtual std::string helper_info(const std::string&) const = 0;

    virtual PicoJsonNode value_info_as_json() const = 0;

    virtual std::string value_info(const std::string&) const = 0;

    virtual std::string value() const = 0;

    virtual Configure to_yaml() const = 0;

    virtual bool check() const = 0;

    void __initialize__() {}
};

// TODO: add to yaml func
class ConfigNode {
public:
    ConfigNode() {}

    virtual bool load_config(const Configure& config);

    virtual std::string helper_info(const std::string& tab);

    virtual PicoJsonNode value_info_as_json();

    virtual std::string value_info(const std::string& tab);

    virtual Configure to_yaml();

    virtual bool check();

    int run_id = 0;

protected:
    using this_type = ConfigNode*;
    std::map<std::string, std::function<bool(this_type, const Configure&, bool)>>
        _inner_config_loader;
    std::map<std::string, std::function<PicoJsonNode(this_type)>> _inner_config_to_json;
    std::map<std::string, std::function<std::string(this_type, const std::string&)>>
        _inner_config_value_func;
    std::vector<std::function<std::string(this_type, const std::string&)>>
        _inner_config_descriptor;
    std::vector<std::function<bool(this_type)>> _inner_config_check_func;
    std::map<std::string, std::function<Configure(this_type)>> _inner_config_to_yaml;

public:
    std::unordered_map<std::string, std::function<void(ConfigNode*)>> _all_conf_map;
    bool _inited = false;

    void __initialize__() {
        if (_inited) return;
        _inited = true;
        for (auto pair : _all_conf_map) {
            pair.second(this);
        }
    }
};// class ConfigNode

class EnumNode : public ConfigNode {
public:
    virtual std::string get_key() {
        return "type";
    }

    virtual bool load_config(const Configure& config) override;

    virtual PicoJsonNode value_info_as_json() override;

    virtual std::string helper_info(const std::string& tab) override;

    virtual std::string value_info(const std::string& tab) override;

    virtual Configure to_yaml() override;

protected:
    using this_type = ConfigNode*;
    std::map<std::string, std::function<bool(this_type, const Configure&)>>
        _inner_enum_config_loader;
    std::map<std::string, std::function<PicoJsonNode(this_type)>>
        _inner_enum_config_to_json;
    std::map<std::string, std::function<std::string(this_type, const std::string&)>>
        _inner_enum_config_value_func;
    std::map<std::string, std::function<bool(this_type)>> _inner_enum_config_check_func;
    std::vector<std::function<std::string(this_type, const std::string&)>>
        _inner_enum_config_descriptor;
    std::map<std::string, std::function<Configure(this_type)>> _inner_enum_config_to_yaml;
    std::string _in_key;
};

template <class T, typename Enable = void>
class ListNode : public ConfigNode {
public:
    ListNode() {}

    ListNode(const std::vector<T>& data) : _data(data) {}

    ListNode(const std::initializer_list<T>& data) : _data(data) {}

    virtual bool load_config(const Configure& config) override {
        if (config.type() != YAML::NodeType::value::Sequence) {
            // RLOG(WARNING) << "ListNode config must be a sequence.";
            _data.resize(1);
            if (!config.try_as<T>(_data[0]))
                return false;
        } else {
            _data.resize(config.size());
            for (size_t i = 0; i < config.size(); ++i) {
                if (!config[i].try_as<T>(_data[i])) return false;
            }
        }
        return true;
    }

    virtual std::string helper_info(const std::string&) override {
        return "";
    }

    virtual PicoJsonNode value_info_as_json() override {
        PicoJsonNode ret;
        for (size_t i = 0; i < _data.size(); ++i) {
            ret.push_back(_data[i]);
        }
        return ret;
    }

    virtual std::string value_info(const std::string& tab) override {
        if (_data.empty()) return "";
        std::string ret;
        for (size_t i = 0; i < _data.size(); ++i) {
            ret += tab + "- " + pico_lexical_cast<std::string>(_data[i]) + "\n";
        }
        if (ret.length() > 0) ret.pop_back();
        return ret;
    }

    virtual Configure to_yaml() override {
        Configure yaml;
        yaml.node() = YAML::Node(YAML::NodeType::Sequence);
        for (auto& val : _data)
            yaml.node().push_back(val);
        return yaml;
    }

    T& operator[](size_t i) {
        SCHECK(i < _data.size());
        return _data[i];
    }

    const T& operator[](size_t i) const {
        SCHECK(i < _data.size());
        return _data[i];
    }

    size_t size() const {
        return _data.size();
    }

    const std::vector<T>& data() const {
        return _data;
    }

    std::vector<T>& data() {
        return _data;
    }

    operator const std::vector<T>&() const {
        return _data;
    }

protected:
    std::vector<T> _data;
};

template <class T>
class ListNode<T, std::enable_if_t<std::is_base_of<ConfigNode, T>::value
    || std::is_base_of<CustomConfigNode, T>::value>> : public ConfigNode {
public:
    ListNode() = default;

    ListNode(const std::vector<T>& data) : _data(data) {}

    ListNode(const std::initializer_list<T>& data) : _data(data) {}

    bool load_config(const Configure& config) override {
        if (config.type() != YAML::NodeType::value::Sequence) {
            // RLOG(WARNING) << "ListNode config must be a sequence.";
            _data.resize(1);
            if (!_data[0].load_config(config))
                return false;
        } else {
            _data.resize(config.size());
            for (size_t i = 0; i < config.size(); ++i) {
                _data[i].__initialize__();
                if (!_data[i].load_config(config[i])) {
                    return false;
                }
            }
        }
        return true;
    }

    std::string helper_info(const std::string& tab) override {
        T tmp;
        tmp.__initialize__();
        std::string ret = tab + "  List of:\n" + tmp.helper_info(tab + "  ");
        return ret;
    }

    virtual PicoJsonNode value_info_as_json() override {
        PicoJsonNode ret;
        for (size_t i = 0; i < _data.size(); ++i) {
            ret.push_back(_data[i].value_info_as_json());
        }
        return ret;
    }

    virtual std::string value_info(const std::string& tab) override {
        if (_data.empty()) return "";
        std::string ret;
        for (size_t i = 0; i < _data.size(); ++i) {
            auto tmp = _data[i].value_info(tab + "    ") + "\n";
            tmp[tab.length()] = '-';
            ret += tmp;
        }
        if (ret.length() > 0) ret.pop_back();
        return ret;
    }

    virtual Configure to_yaml() override {
        Configure yaml;
        yaml.node() = YAML::Node(YAML::NodeType::Sequence);
        for (auto& val : _data) {
            auto tmp = val.to_yaml();
            if (tmp.type() != YAML::NodeType::value::Null)
                yaml.node().push_back(tmp.node());
        }
        return yaml;
    }

    T& operator[](size_t i) {
        SCHECK(i < _data.size());
        return _data[i];
    }

    const T& operator[](size_t i) const {
        SCHECK(i < _data.size());
        return _data[i];
    }

    size_t size() const {
        return _data.size();
    }

    const std::vector<T>& data() const {
        return _data;
    }

    std::vector<T>& data() {
        return _data;
    }

    operator const std::vector<T>&() const {
        return _data;
    }

protected:
    std::vector<T> _data;
};

class ApplicationConfigureHelper {
public:
    void register_node(const std::string& key, ConfigNode* node) {
        _config_nodes[key] = node;
    }

    void register_desc(const std::string& key, const std::string& desc) {
        _config_desc[key] = desc;
    }

    std::string value_info_as_json();

    std::string value_info();

    std::string helper_info();

private:
    std::map<std::string, ConfigNode*> _config_nodes;
    std::map<std::string, std::string> _config_desc;
};

inline ApplicationConfigureHelper& inner_app_struct_config_helper() {
    static ApplicationConfigureHelper app_cfgh;
    return app_cfgh;
}

inline std::string pico_app_struct_config_value_json() {
    return inner_app_struct_config_helper().value_info_as_json();
}

inline void pico_show_app_struct_config_value() {
    SLOG(INFO) << "application related configure:\n"
               << inner_app_struct_config_helper().value_info();
}

inline void pico_show_app_struct_config(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            std::cerr << "application related configure:\n";
            std::cerr << inner_app_struct_config_helper().helper_info() << "\n\n";
            break;
        }
    }
}

namespace ConfigureHelper {

template<class T>
std::enable_if_t<!std::is_base_of<ConfigNode, T>::value, void>
inline conf_declare_initialize(T& ) {}

template<class T>
std::enable_if_t<std::is_base_of<ConfigNode, T>::value, void>
inline conf_declare_initialize(T& x) {
    x.__initialize__();
}

template<class T>
std::enable_if_t<std::is_base_of<CustomConfigNode, T>::value, Configure>
inline conf_to_yaml(T& x) {
    return x.to_yaml();
}

template<class T>
std::enable_if_t<!std::is_base_of<CustomConfigNode, T>::value, Configure>
inline conf_to_yaml(T& x) {
    Configure ret;
    ret.node() = x;
    return ret;
}

template <typename T>
inline PicoJsonNode template_unit_to_json(const ConfigUnit& unit, const T& value) {
    PicoJsonNode ret;
    ret.add("type", unit.type);
    ret.add("description", unit.desc);
    ret.add("is_missing_ok", unit.is_missing_ok);
    if (unit.is_missing_ok) {
        ret.add("default_value", unit.defv);
    }
    ret.add("checker", unit.checker_info);
    ret.add("is_structure", unit.is_structure);
    ret.add("value", value);
    return ret;
}

PicoJsonNode unit_to_json(const ConfigUnit& unit);

template <typename T>
inline std::enable_if_t<!std::is_base_of<ConfigNode, T>::value &&
                        !std::is_base_of<CustomConfigNode, T>::value, bool>
template_config_loader(const Configure& conf, T& tar, const std::string& name,
                       bool is_missing, bool is_missing_ok) {
    if (is_missing) {
        if (is_missing_ok) return true;
        ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER))
            << "required configure [" << name << "] missing value.";
        return false;
    }
    bool ret = conf.try_as<T>(tar);
    if (!ret)
        RLOG(WARNING) << "lexical cast config [" << name << "] into ["
                      << readable_typename<T>() << "] failed. value: [" << conf.dump()
                      << "]";
    return ret;
}

template <typename T>
inline std::enable_if_t<std::is_base_of<ConfigNode, T>::value, bool>
template_config_loader(const Configure& conf, T& tar, const std::string& name,
                       bool is_missing, bool is_missing_ok) {
    if (is_missing) {
        if (is_missing_ok) return true;
        ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER))
            << "required configure [" << name << "] missing value.";
        return false;
    }
    bool ret = tar.load_config(conf);
    if (!ret)
        RLOG(WARNING) << "load config [" << name << "] into ["
                      << readable_typename<T>() << "] failed. value: [" << conf.dump()
                      << "]";
    return ret;
}

template <typename T>
inline std::enable_if_t<std::is_base_of<CustomConfigNode, T>::value, bool>
template_config_loader(const Configure& conf, T& tar, const std::string& name,
                       bool is_missing, bool is_missing_ok) {
    if (is_missing) {
        if (is_missing_ok) {
            tar.init_config();
            return true;
        }
        ELOG(WARNING, PICO_CORE_ERRCODE(CFG_CHECKER))
            << "required configure [" << name << "] missing value.";
        return false;
    }
    bool ret = tar.load_config(conf);
    if (!ret)
        RLOG(WARNING) << "load config [" << name << "] into ["
                      << readable_typename<T>() << "] failed. value: [" << conf.dump()
                      << "]";
    return ret;
}

template <typename T>
inline std::enable_if_t<!std::is_base_of<CustomConfigNode, T>::value, std::string>
template_config_value(const T& tar) {
    return paradigm4::pico::core::pico_lexical_cast<std::string>(tar);
}

template <typename T>
inline std::enable_if_t<std::is_base_of<CustomConfigNode, T>::value, std::string>
template_config_value(const T& tar) {
    return tar.value();
}

template <typename T>
inline bool template_config_checker(
    const T& tar, std::string name,
    std::function<bool(const T&, const std::string&)> func) {
    return func(tar, name);
}

template <typename T>
inline std::enable_if_t<!std::is_base_of<CustomConfigNode, T>::value, std::string>
template_helper_info(const std::string& tab, const ConfigUnit& unit, const T&) {
    return tab + unit.helper_info();
}

template <typename T>
inline std::enable_if_t<std::is_base_of<CustomConfigNode, T>::value, std::string>
template_helper_info(const std::string& tab, const ConfigUnit& unit, const T& val) {
    return tab + unit.helper_info() + "\n" + val.helper_info(tab);
}

bool helper_info_cmp(const std::string& a, const std::string& b);

}

#define DECLARE_CONFIG(name, type) \
class name: public type

#define DEFINE_CONFIG(name, ...)   \
    name::name()

#ifndef PICO_APP_CONFIG_REGISTER_HELPER_INFO
#define PICO_APP_CONFIG_REGISTER_HELPER_INFO(name, type)                                \
    bool dummy_##type##_register PICO_UNUSED = []() -> bool {                           \
        type tmp_conf = type();                                                         \
        tmp_conf.__initialize__();                                                      \
        inner_app_struct_config_helper().register_desc(                                 \
            #name, tmp_conf.helper_info(""));                                           \
        return true;                                                                    \
    }();
#endif

#ifndef PICO_APP_CONFIG_REGISTER_VALUE_INFO
#define PICO_APP_CONFIG_REGISTER_VALUE_INFO(name, value) \
    inner_app_struct_config_helper().register_node(name, &value);
#endif

#ifndef PICO_CONFIGURE_CONTENT
#define PICO_CONFIGURE_CONTENT(this_type,                                               \
        name, type, deftval, desc, is_missing_ok, cfg_checker)                          \
    name##_unit = ConfigUnit(#name, #type, desc,                                        \
            paradigm4::pico::core::pico_lexical_cast<std::string>(deftval),                   \
            is_missing_ok, false, cfg_checker.tostring());                              \
    auto inner_config_load_func = [](                                                   \
            ConfigNode* base_ptr, const Configure& conf, bool is_missing) {             \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        return ConfigureHelper::template_config_loader<type>(                           \
                conf, ptr->name, #name, is_missing, is_missing_ok);                     \
    };                                                                                  \
    _inner_config_loader[#name] = inner_config_load_func;                               \
    auto inner_config_to_json_func = [](ConfigNode* base_ptr) {                         \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        PicoJsonNode ret = ConfigureHelper::template_unit_to_json(                      \
            ptr->name##_unit, ConfigureHelper::template_config_value(ptr->name));       \
        return ret;                                                                     \
    };                                                                                  \
    _inner_config_to_json[#name] = inner_config_to_json_func;                           \
    auto inner_config_to_yaml_func = [](ConfigNode* base_ptr) {                         \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        return ConfigureHelper::conf_to_yaml(ptr->name);                                \
    };                                                                                  \
    _inner_config_to_yaml[#name] = inner_config_to_yaml_func;                           \
    static auto inner_checker = cfg_checker;                                            \
    auto inner_config_check_func = [](ConfigNode* base_ptr) {                           \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        return inner_checker.checker()(ptr->name, #name);                               \
    };                                                                                  \
    _inner_config_check_func.push_back(inner_config_check_func);                        \
    auto inner_config_value_func = [](ConfigNode* base_ptr, const std::string&) {       \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        auto ret = ConfigureHelper::template_config_value(ptr->name);                   \
        return ret.size() == 0 ? "\"\"" : ret;                                          \
    };                                                                                  \
    _inner_config_value_func[#name] = inner_config_value_func;                          \
    auto inner_config_descriptor = [](ConfigNode* base_ptr, const std::string& tab) {   \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        return ConfigureHelper::template_helper_info(tab, ptr->name##_unit, ptr->name); \
    };                                                                                  \
    _inner_config_descriptor.push_back(inner_config_descriptor);
#endif

#ifndef PICO_STRUCT_CONFIGURE_CONTENT
#define PICO_STRUCT_CONFIGURE_CONTENT(this_type,                                        \
        name, type, desc, is_missing_ok, cfg_checker, deftval...)                       \
    name##_unit = ConfigUnit(#name, #type, desc, #deftval,                              \
            is_missing_ok, true, cfg_checker.tostring());                               \
    auto inner_config_load_func = [](                                                   \
            ConfigNode* base_ptr, const Configure& conf, bool is_missing) {             \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        return ConfigureHelper::template_config_loader<type>(                           \
                conf, ptr->name, #name, is_missing, is_missing_ok);                     \
    };                                                                                  \
    _inner_config_loader[#name] = inner_config_load_func;                               \
    auto inner_config_to_json_func = [](ConfigNode* base_ptr) {                         \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        PicoJsonNode ret = ConfigureHelper::template_unit_to_json(                      \
            ptr->name##_unit, ptr->name.value_info_as_json());                          \
        return ret;                                                                     \
    };                                                                                  \
    _inner_config_to_json[#name] = inner_config_to_json_func;                           \
    auto inner_config_to_yaml_func = [](ConfigNode* base_ptr) {                         \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        return ptr->name.to_yaml();                                                     \
    };                                                                                  \
    _inner_config_to_yaml[#name] = inner_config_to_yaml_func;                           \
    static auto inner_checker = cfg_checker;                                            \
    auto inner_config_check_func = [](ConfigNode* base_ptr) {                           \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        if (!ptr->name.check()) return false;                                           \
        if (!inner_checker.checker()(ptr->name, #name)) return false;                   \
        return true;                                                                    \
    };                                                                                  \
    _inner_config_check_func.push_back(inner_config_check_func);                        \
    auto inner_config_value_func = [](                                                  \
            ConfigNode* base_ptr, const std::string& tab) {                             \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        return "\n" + ptr->name.value_info(tab + "    ");                               \
    };                                                                                  \
    _inner_config_value_func[#name] = inner_config_value_func;                          \
    auto inner_config_descriptor = [](ConfigNode* base_ptr, const std::string& tab) {   \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        std::string helper_info = ptr->name.helper_info(tab);                           \
        if (helper_info != "") helper_info = "\n" + helper_info;                        \
        if (!is_missing_ok)                                                             \
            return tab + "Required: " + #name + " = " + #deftval + ", " + #type +       \
                   ", " + desc + ", constraint: " + cfg_checker.tostring() +            \
                   helper_info;                                                         \
        return tab + "Optional: " + #name ", " + #type + ", " + desc +                  \
               ", constraint: " + cfg_checker.tostring() + helper_info;                 \
    };                                                                                  \
    _inner_config_descriptor.push_back(inner_config_descriptor);
#endif // PICO_STRUCT_CONFIGURE_CONTENT

#ifndef PICO_ENUM_CONFIGURE_CONTENT
#define PICO_ENUM_CONFIGURE_CONTENT(this_type, name, key, type, desc, cfg_checker)      \
    name##_unit = ConfigUnit(key, #type, desc, "",                                      \
            false, true, cfg_checker.tostring());                                       \
    auto inner_config_load_func = [](ConfigNode* base_ptr, const Configure& conf) {     \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        return ptr->name.load_config(conf);                                             \
    };                                                                                  \
    _inner_enum_config_loader[key] = inner_config_load_func;                            \
    auto inner_config_to_json_func = [](ConfigNode* base_ptr) {                         \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        PicoJsonNode ret = ConfigureHelper::template_unit_to_json(                      \
            ptr->name##_unit, ptr->name.value_info_as_json());                          \
        return ret;                                                                     \
    };                                                                                  \
    _inner_enum_config_to_json[key] = inner_config_to_json_func;                        \
    static auto inner_checker = cfg_checker;                                            \
    auto inner_config_check_func = [](ConfigNode* base_ptr) {                           \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        if (!ptr->name.check()) return false;                                           \
        if (!inner_checker.checker()(ptr->name, key)) return false;                     \
        return true;                                                                    \
    };                                                                                  \
    _inner_enum_config_check_func[key] = inner_config_check_func;                       \
    auto inner_config_to_yaml_func = [](ConfigNode* base_ptr) {                         \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        return ptr->name.to_yaml();                                                     \
    };                                                                                  \
    _inner_enum_config_to_yaml[key] = inner_config_to_yaml_func;                        \
    auto inner_config_value_func = [](                                                  \
            ConfigNode* base_ptr, const std::string& tab) {                             \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        return "\n" + ptr->name.value_info(tab);                                        \
    };                                                                                  \
    _inner_enum_config_value_func[key] = inner_config_value_func;                       \
    auto inner_config_descriptor = [](ConfigNode* base_ptr, const std::string& tab) {   \
        auto ptr = static_cast<this_type*>(base_ptr);                                   \
        auto helper_info = ptr->name.helper_info(tab);                                  \
        if (helper_info != "") helper_info = "\n" + helper_info;                        \
        return tab + key + ", " + #type + ", " + desc +                                 \
               ", constraint: " + cfg_checker.tostring() + helper_info;                 \
    };                                                                                  \
    _inner_enum_config_descriptor.push_back(inner_config_descriptor);
#endif // PICO_ENUM_CONFIGURE_DEFINE

#ifndef PICO_CONFIGURE_COMMON_DECLARE
#define PICO_CONFIGURE_COMMON_DECLARE(type, name)                                       \
public:                                                                                 \
    type name;                                                                          \
private:                                                                                \
    ConfigUnit name##_unit;                                                             \
    bool name##_dummy_register = [](ConfigNode* ptr) -> bool {                          \
        using this_type = decltype(this);                                               \
        ptr->_all_conf_map[#name] = [](ConfigNode* ptr) {                               \
                static_cast<this_type>(ptr)->name##_define_func();                      \
            };                                                                          \
        static_cast<this_type>(ptr)->name##_set_default_value();                        \
        return false;                                                                   \
    }(this);
#endif

#ifndef PICO_CONFIGURE_DECLARE
#define PICO_CONFIGURE_DECLARE(type, name)                                              \
    PICO_CONFIGURE_COMMON_DECLARE(type, name);                                          \
    void name##_define_func();                                                          \
    void name##_set_default_value();
#endif

#ifndef PICO_CONFIGURE_DEFINE
#define PICO_CONFIGURE_DEFINE(                                                          \
        class_type, name, type, deftval, desc, is_missing_ok, cfg_checker)              \
    void class_type::name##_define_func() {                                             \
        PICO_CONFIGURE_CONTENT(class_type,                                              \
                name, type, deftval, desc, is_missing_ok, cfg_checker);                 \
    }                                                                                   \
    void class_type::name##_set_default_value() {                                       \
        name = type(deftval);                                                           \
    }
#endif // PICO_CONFIGURE_DEFINE

#ifndef PICO_STRUCT_CONFIGURE_DEFINE
#define PICO_STRUCT_CONFIGURE_DEFINE(                                                   \
        class_type, name, type, desc, is_missing_ok, cfg_checker, deftval...)           \
    void class_type::name##_define_func() {                                             \
        PICO_STRUCT_CONFIGURE_CONTENT(class_type,                                       \
                name, type, desc, is_missing_ok, cfg_checker, deftval);                 \
    }                                                                                   \
    void class_type::name##_set_default_value() {                                       \
        name = type(deftval);                                                           \
    }
#endif // PICO_STRUCT_CONFIGURE_DEFINE

#ifndef PICO_ENUM_CONFIGURE_DEFINE
#define PICO_ENUM_CONFIGURE_DEFINE(class_type, name, key, type, desc, cfg_checker)      \
    void class_type::name##_set_default_value() {}                                      \
    void class_type::name##_define_func() {                                             \
        PICO_ENUM_CONFIGURE_CONTENT(class_type, name, key, type, desc, cfg_checker)     \
    }
#endif // PICO_ENUM_CONFIGURE_DEFINE

#ifndef PICO_INNER_APP_CONFIG
#define PICO_INNER_APP_CONFIG(name, itype, deftval, desc, is_missing_ok, cfg_checker)   \
    PICO_CONFIGURE_COMMON_DECLARE(itype, name);                                         \
    void name##_define_func() {                                                         \
        using class_type = typename std::remove_pointer<decltype(this)>::type;          \
        PICO_CONFIGURE_CONTENT(class_type,                                              \
                name, itype, deftval, desc, is_missing_ok, cfg_checker);                \
    }                                                                                   \
    void name##_set_default_value() {                                                   \
        name = itype(deftval);                                                          \
    }
#endif // PICO_INNER_APP_CONFIG

#ifndef PICO_INNER_APP_STRUCT_CONFIG
#define PICO_INNER_APP_STRUCT_CONFIG(                                                   \
        name, itype, desc, is_missing_ok, cfg_checker, deftval...)                      \
    PICO_CONFIGURE_COMMON_DECLARE(itype, name);                                         \
    static_assert(std::is_base_of<ConfigNode, itype>::value ||                          \
                      std::is_base_of<CustomConfigNode, itype>::value,                  \
                  "template is not base from ConfigNode");                              \
    void name##_define_func() {                                                         \
        using class_type = std::remove_pointer<decltype(this)>::type;                   \
        PICO_STRUCT_CONFIGURE_CONTENT(class_type,                                       \
            name, itype, desc, is_missing_ok, cfg_checker, deftval);                    \
    }                                                                                   \
    void name##_set_default_value() {                                                   \
        name = itype(deftval);                                                          \
    }
#endif // PICO_INNER_APP_STRUCT_CONFIG

#ifndef PICO_INNER_APP_ENUM_CONFIG
#define PICO_INNER_APP_ENUM_CONFIG(name, key, itype, desc, cfg_checker)                 \
    PICO_CONFIGURE_COMMON_DECLARE(itype, name);                                         \
    static_assert(std::is_base_of<ConfigNode, itype>::value,                            \
                  "template is not base from ConfigNode");                              \
    void name##_set_default_value() {}                                                  \
    void name##_define_func() {                                                         \
        using class_type = std::remove_pointer<decltype(this)>::type;                   \
        PICO_ENUM_CONFIGURE_CONTENT(class_type, name, key, itype, desc, cfg_checker);   \
    }
#endif // PICO_INNER_APP_ENUM_CONFIG

}
} // namespace pico
} // namespace paradigm4
#include "configure_checkers.h"

#endif
