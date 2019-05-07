#ifndef PARADIGM4_PICO_COMMON_URI_CONFIG_H
#define PARADIGM4_PICO_COMMON_URI_CONFIG_H

#include <string>
#include <map>
#include "common/include/URICode.h"
#include "common/include/DsConfigDef.h"
#include "common/include/ConfigureHelper.h"
#include "common/include/configure_checkers.h"
#include "common/include/Archive.h"

namespace paradigm4 {
namespace pico {

PICO_ENUM_SERIALIZATION(FileSystemType, int8_t);

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

class ds_config_t {
    //<key, <val, setlvl>>
    std::map<std::string, std::pair<std::string, int8_t>> params;

    PICO_SERIALIZATION(params);
public:
    ds_config_t() {
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

    ds_config_t& config() {
        return _config;
    }

    const ds_config_t& config() const {
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

    operator std::string() const {
        return uri();
    }

protected:
    std::string _prefix = ""; //extra prefix for protocol
    std::string _name = ""; //real name used
    FileSystemType _store = FileSystemType::UNKNOWN;
    ds_config_t _config;

    PICO_SERIALIZATION(_prefix, _name, _store, _config);
};

class DsURIConfig : public URIConfig {
public:
    DsURIConfig() : URIConfig() {} 

    DsURIConfig(const char* s) : URIConfig(s) {}

    virtual void init_config() override {
        if (valid())
            fill_default();
    }

    virtual bool load_config(const Configure& config) override;

    void fill_default();
    
    virtual std::string helper_info(const std::string& tab) const override;

    virtual PicoJsonNode value_info_as_json() const override;

    virtual std::string value_info(const std::string& tab) const override;

    virtual std::string value() const override;

    virtual bool check() const override;
    
    virtual DsFormat& ds_format() const = 0;
};

class TrainInputURI : public DsURIConfig {
public:
    using DsURIConfig::DsURIConfig;
    virtual DsFormat& ds_format() const override {
        static DsFormat dsf(
                {URIStorageLocalFile(), 
                    URIStorageHdfs(), 
                    URIStorageKafka()},
                {URIFormatTrainInputGc(), 
                    URIFormatInputAvro(), 
                    URIFormatArchive()},
                URIParamsDataSource());
        return dsf;
    }

    friend std::ostream& operator<<(std::ostream& out, const TrainInputURI& key) {
        out << key.value();
        return out;
    }
};

class ValidInputURI : public DsURIConfig {
public:
    using DsURIConfig::DsURIConfig;
    virtual DsFormat& ds_format() const {
        static DsFormat dsf(
                {URIStorageLocalFile(),
                    URIStorageHdfs(),
                    URIStorageKafka()},
                {URIFormatValidInputGc(),
                    URIFormatInputAvro(),
                    URIFormatArchive()},
                URIParamsDataSource());
        return dsf;
    }
};

class PredInputURI : public DsURIConfig {
public:
    using DsURIConfig::DsURIConfig;
    virtual DsFormat& ds_format() const {
        static DsFormat dsf(
                {URIStorageLocalFile(),
                    URIStorageHdfs(),
                    URIStorageKafka()},
                {URIFormatPredInputGc(),
                    URIFormatInputAvro(),
                    URIFormatArchive()},
                URIParamsDataSource());
        return dsf;
    }
};

class PredOutputURI : public DsURIConfig {
public:
    using DsURIConfig::DsURIConfig;
    virtual DsFormat& ds_format() const {
        static DsFormat dsf(
                {URIStorageLocalFile(),
                    URIStorageHdfs()},
                {URIFormatOutputGc(),
                    URIFormatOutputAvro(),
                    URIFormatArchive()},
                URIParamsDataSink());
        return dsf;
    }
};

class ModelInputURI : public DsURIConfig {
public:
    using DsURIConfig::DsURIConfig;
    virtual DsFormat& ds_format() const {
        static DsFormat dsf(
                {URIStorageLocalFile(),
                    URIStorageHdfs()},
                {URIFormatInputAvro(),
                    URIFormatArchive(), 
                    URIFormatArchiveLine()});
        return dsf;
    }
};

class ModelOutputURI : public DsURIConfig {
public:
    using DsURIConfig::DsURIConfig;
    virtual DsFormat& ds_format() const {
        static DsFormat dsf(
                {URIStorageLocalFile(),
                    URIStorageHdfs()},
                {URIFormatOutputAvro(),
                    URIFormatArchive(), 
                    URIFormatArchiveLine()});
        return dsf;
    }
};

class CacheURI : public DsURIConfig {
public:
    using DsURIConfig::DsURIConfig;
    virtual DsFormat& ds_format() const {
        static DsFormat dsf(
                {URIStorageLocalFile(),
                    URIStorageMem()},
                {URIFormatArchive(), 
                    URIFormatMem()},
                {URIParamsCache()});
        return dsf;
    }
};

class RawURI : public DsURIConfig {
public:
    using DsURIConfig::DsURIConfig;
    virtual DsFormat& ds_format() const {
        static DsFormat dsf(
                {URIStorageLocalFile(),
                    URIStorageHdfs()},
                {},
                URIParams(),
                true);
        return dsf;
    }
};

template<class T>
class URIChecker {
public:
    std::function<bool(const T&, const std::string&)> checker() const {
        return [](const T& uri, const std::string& name) ->bool {
            if (uri.check())
                return true;
            RLOG(WARNING) << name << "[" << uri.uri() << "] check failed";
            return false;
        };
    }

    std::string tostring() const {
        return "uri check";
    }
};

template<class T>
class URIValidChecker {
public:
    std::function<bool(const T&, const std::string&)> checker() const {
        return [](const T& uri, const std::string& name) -> bool {
            if (uri.valid() && uri.check())
                return true;
            RLOG(WARNING) << name << "[" << uri.uri() << "] valid check failed";
            return false;
        };
    }

    std::string tostring() const {
        return "uri valid check";
    }
};
  
template<typename T>
using ListURIChecker = ListNodeChecker<URIChecker<T>, T>;

template<typename T>
using ListURIValidChecker = ListNodeChecker<URIValidChecker<T>, T>;

template<typename T>
using NonEmptyListURIChecker = NonEmptyListNodeChecker<URIChecker<T>, T>;

template<typename T>
using NonEmptyListURIValidChecker = NonEmptyListNodeChecker<URIValidChecker<T>, T>;

const std::vector<std::string> uri_vector_match_keys = {
    DS_FORMAT,
    DS_IS_ORDERED,
    DS_BLOCK_SIZE,
    DS_MINI_BATCH_SIZE,
    DS_IS_USE_GLOBAL_SHUFFLE,
    DS_GLOBAL_SHUFFLE_RETRY_PER_MS,
    DS_TRAINING_MODE,
    DS_IO_CONCURRENCY,
    DS_ARCHIVE_COMPRESS,
    DS_ARCHIVE_COMPRESS,
    DS_ARCHIVE_IS_TEXT,
    DS_INS_IS_COPY,
    DS_INS_IS_CHECK_DUP,
    DS_INS_IS_ALLOW_ERR,
    DS_INS_IS_ALLOW_MISSING_LABEL,
    DS_INS_ERR_ACC_NAME,
    DS_INS_ENABLED_SLOTS
};

template<class URI>
void pico_uris_checker(const std::vector<URI>& uris) {
    for (size_t i = 1; i < uris.size(); ++i) {
        SCHECK(uris[i].storage_type() == uris[0].storage_type()) 
            << uris[i].uri() <<  " mismatch " << uris[0].uri() << " storage";
        for (size_t j = 0; j < uri_vector_match_keys.size(); ++j) {
            auto& key = uri_vector_match_keys[j];
            std::string s1;
            std::string s2;
            bool r1 = uris[i].config().get_val(key, s1);
            bool r2 = uris[0].config().get_val(key, s2);
            SCHECK(r1 == r2 && (!r1 || s1 == s2))
                << uris[i].uri() << " mismatch " << uris[0].uri()
                << " on param:" << key;
        }
    }
}

}
}



#endif //PARADIGM4_PICO_COMMON_URI_CONFIG_H
