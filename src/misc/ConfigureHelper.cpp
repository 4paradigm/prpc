
#include "ConfigureHelper.h"

namespace paradigm4 {
namespace pico {
namespace core {

bool ConfigNode::load_config(const Configure& config) {
    __initialize__();
    for (auto& pair: _inner_config_loader) {
        if (!pair.second(this, config[pair.first], !config.has(pair.first))) {
            RLOG(WARNING) << "load " << pair.first << " failed.";
            return false;
        }
    }
    if (!check()) return false;
    return true;
}

std::string ConfigNode::helper_info(const std::string& tab) {
    __initialize__();
    std::string ret;
    std::vector<std::string> tmp;
    for (auto& func: _inner_config_descriptor) {
        tmp.push_back(func(this, tab + "  "));
    }
    sort(tmp.begin(), tmp.end(), ConfigureHelper::helper_info_cmp);
    for (auto& str: tmp) {
        ret += str + "\n";
    }
    if (ret.length() > 0) ret.pop_back();
    return ret;
}

PicoJsonNode ConfigNode::value_info_as_json() {
    __initialize__();
    PicoJsonNode ret;
    for (auto& pair: _inner_config_to_json) {
        ret.add(pair.first, pair.second(this));
    }
    return ret;
}

std::string ConfigNode::value_info(const std::string& tab) {
    __initialize__();
    std::string ret;
    for (auto& pair: _inner_config_value_func) {
        ret += tab + pair.first + " : " + pair.second(this, tab) + "\n";
    }
    if (ret.length() > 0) ret.pop_back();
    return ret;
}

Configure ConfigNode::to_yaml() {
    __initialize__();
    Configure yaml;
    for (auto& pair : _inner_config_to_yaml) {
        auto tmp = pair.second(this);
        if (tmp.type() != YAML::NodeType::value::Null)
            yaml.node()[pair.first] = tmp.node();
    }
    return yaml;
}

bool ConfigNode::check() {
    __initialize__();
    for (auto& func: _inner_config_check_func) {
        if (!func(this)) return false;
    }
    return true;
}

std::string ApplicationConfigureHelper::value_info_as_json() {
    PicoJsonNode json;
    for (auto& pair: _config_nodes) {
        json.add(pair.first, pair.second->value_info_as_json());
    }
    std::string ret;
    json.save(ret);
    return ret;
}

std::string ApplicationConfigureHelper::helper_info() {
    std::string ret;
    for (auto& pair: _config_desc) {
        ret += pair.first + ":\n" + pair.second + "\n";
    }
    if (ret.length() > 0) ret.pop_back();
    return ret;
}

std::string ApplicationConfigureHelper::value_info() {
    std::string ret;
    for (auto& pair: _config_nodes) {
        ret += pair.first + ":\n" + pair.second->value_info("    ");
    }
    return ret;
}

bool EnumNode::load_config(const Configure& config) {
    if (!ConfigNode::load_config(config)) return false;

    std::string key = get_key();
    _in_key = config[key].as<std::string>();
    if(!(_inner_enum_config_loader.count(_in_key) &&
         _inner_enum_config_check_func.count(_in_key))){
        return true;
    }

    if (!_inner_enum_config_loader[_in_key](this, config)) return false;
    if (!_inner_enum_config_check_func[_in_key](this)) return false;
    return true;
}

PicoJsonNode EnumNode::value_info_as_json() {
    PicoJsonNode ret = ConfigNode::value_info_as_json();
    if(_inner_enum_config_to_json.find(_in_key)==_inner_enum_config_to_json.end())
        return ret;
    PicoJsonNode enm = _inner_enum_config_to_json.find(_in_key)->second(this);
    ret.add(_in_key, enm);
    return ret;
}

std::string EnumNode::helper_info(const std::string& tab) {
    std::string ret = ConfigNode::helper_info(tab) + "\n" + tab + "  Enum of:\n";
    std::vector<std::string> tmp;
    for (auto func: _inner_enum_config_descriptor) {
        tmp.push_back(func(this, tab + "  ") + "\n");
    }
    std::sort(tmp.begin(), tmp.end(), ConfigureHelper::helper_info_cmp);
    for (auto& str: tmp) {
        ret += str;
    }
    if (ret.length() > 0) ret.pop_back();
    return ret;
}

std::string EnumNode::value_info(const std::string& tab) {
    std::string ret = ConfigNode::value_info(tab);
    auto enum_value = _inner_enum_config_value_func.find(_in_key);
    if (enum_value != _inner_enum_config_value_func.end())
        ret += enum_value->second(this, tab);
    return ret;
}

Configure EnumNode::to_yaml() {
    auto ret = ConfigNode::to_yaml();
    auto tmp = _inner_enum_config_to_yaml[_in_key](this);

    ret.node()[get_key()] = _in_key;
    for (auto it : tmp.node()) {
        ret.node()[it.first] = it.second;
    }
    return ret;
}

template class ListNode<std::string, void>;
template class ListNode<int32_t, void>;
template class ListNode<size_t, void>;

namespace ConfigureHelper {

PicoJsonNode unit_to_json(const ConfigUnit& unit) {
    PicoJsonNode ret;
    ret.add("type", unit.type);
    ret.add("description", unit.desc);
    ret.add("is_missing_ok", unit.is_missing_ok);
    if (unit.is_missing_ok) {
        ret.add("default_value", unit.defv);
    }
    ret.add("checker", unit.checker_info);
    ret.add("is_structure", unit.is_structure);
    return ret;
}

bool helper_info_cmp(const std::string& a, const std::string& b) {
    int oft = 0;
    while (a[oft] == ' ')
        ++oft;
    if (a[oft] != b[oft]) return a[oft] > b[oft];
    return strcmp(a.c_str() + oft + 10, b.c_str() + oft + 10) < 0;
}

} // namespace ConfigureHelper

}
} // namespace pico
} // namespace paradigm4

