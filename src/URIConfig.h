#ifndef PARADIGM4_PICO_COMMON_URI_CONFIG_H
#define PARADIGM4_PICO_COMMON_URI_CONFIG_H

#include <string>
#include <map>

#include "URICode.h"
#include "ConfigureHelper.h"
#include "ShellUtility.h"

namespace paradigm4 {
namespace pico {
namespace core {

/**
 * uri source level, high level can cover low level
 */
enum URILVL : int8_t {
    DEFAULT = 0, // default value
    URICFG = 1, // uri config
    EXTCFG = 2, // external config
    OVERRIDE = 99, // must override
};
PICO_ENUM_SERIALIZATION(URILVL, int8_t);

class uri_config_t {
    //<key, <val, setlvl>>
    std::map<std::string, std::pair<std::string, int8_t>> params;

    PICO_SERIALIZATION(params);
public:
    uri_config_t() {
    }

    template<class T>
    bool get_val(const std::string& key, T& val) const {
        auto iter = params.find(key);
        if (iter == params.end())
            return false;
        return pico_lexical_cast<T>(iter->second.first, val);
    }

    template<class T>
    bool set_val(const std::string& key, const T& val, int lvl = URILVL::OVERRIDE) {
        std::string sval = pico_lexical_cast<std::string>(val);
        auto ret = params.insert({key, {sval, lvl}});
        if (!ret.second && lvl >= ret.first->second.second) {
            ret.first->second.first = sval;
            ret.first->second.second = lvl;
            return true;
        }
        return ret.second;
    }

    template<class T>
    T get(const std::string& key) const {
        auto iter = params.find(key);
        if (iter == params.end()) {
            SLOG(FATAL) << to_string() << " not find " << key;
            return T();
        }
        return pico_lexical_cast<T>(iter->second.first);
    }

    template<class T>
    T get_with_default(const std::string& key, const T& val = T()) const {
        auto iter = params.find(key);
        if (iter == params.end()) {
            return val;
        }
        return pico_lexical_cast<T>(iter->second.first);
    }

    std::string to_string(int low_lvl = 0) const;

    void clear() {
        params.clear();
    }
};

class URIConfig : public CustomConfigNode {
public:
    URIConfig() = default;

    //URIConfig(const char* uri) {
        //SCHECK(set_uri(uri));
    //}

    URIConfig(const std::string& uri) {
        SCHECK(set_uri(uri));
    }

    URIConfig(const URIConfig& uc) {
        *this = uc;
    }

    URIConfig(URIConfig&& uc) {
        _prefix = std::move(uc._prefix);
        _name = std::move(uc._name);
        _store = uc._store;
        _config = std::move(uc._config);
        uc.clear();
    }

    URIConfig(const URIConfig& path, const std::string& name) : URIConfig(path) {
        _name = _name + name;
    }

    URIConfig& operator=(const URIConfig& uc) {
        _prefix = uc._prefix;
        _name = uc._name;
        _store = uc._store;
        _config = uc._config;
        return *this;
    }

    URIConfig operator+(const std::string& name) const {
        URIConfig ret(*this, name);
        return ret;
    }

    bool set_uri(const std::string& uri);

    void clear() {
        _store = FileSystemType::UNKNOWN;
        _prefix = "";
        _name = "";
        _config.clear();
    }

    FileSystemType storage_type() const {
        return _store;
    }

    bool is_file() const {
        return _store == FileSystemType::LOCAL || _store == FileSystemType::HDFS;
    }

    bool is_memory() const {
        return _store == FileSystemType::MEM;
    }

    std::string uri() const {
        std::string str_param = _config.to_string();
        return _prefix + _name + str_param;
    }

    std::string new_uri(const std::string& name) const {
        URIConfig ret(*this);
        ret.set_name(name);
        return ret.uri();
    }

    std::string append_uri(const std::string& name) const {
        URIConfig ret(*this, name);
        return ret.uri();
    }

    const std::string& name() const {
        return _name;
    }

    const std::string& prefix() const {
        return _prefix;
    }

    void set_name(const std::string& name) {
        _name = name;
    }

    void append_name(const std::string& name) {
        _name = _name + name;
    }

    uri_config_t& config() {
        return _config;
    }

    const uri_config_t& config() const {
        return _config;
    }

    bool valid() const {
        return _store != FileSystemType::UNKNOWN;
    }

    void replace_param(const std::map<std::string, std::pair<std::string, int>>& params);

public:
    virtual void init_config() override {
    }

    virtual bool load_config(const Configure&) override {
        return true;
    }
    
    virtual std::string helper_info(const std::string&) const override {
        return "";
    }

    virtual PicoJsonNode value_info_as_json() const override {
        return "";
    }

    virtual std::string value_info(const std::string&) const override {
        return "";
    }

    virtual std::string value() const override;

    virtual bool check() const override {
        return true;
    }

    virtual Configure to_yaml() const override {
        Configure yaml;
        yaml.node() = uri();
        return yaml;
    }

    //operator std::string() const {
    //    return uri();
    //}

protected:
    std::string _prefix = ""; //extra prefix for protocol
    std::string _name = ""; //real name used
    FileSystemType _store = FileSystemType::UNKNOWN;
    uri_config_t _config;

    PICO_SERIALIZATION(_prefix, _name, _store, _config);
};

}
}
}



#endif //PARADIGM4_PICO_COMMON_URI_CONFIG_H
