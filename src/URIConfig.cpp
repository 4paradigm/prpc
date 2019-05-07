#include <boost/algorithm/string.hpp>
#include "URIConfig.h"

namespace paradigm4 {
namespace pico {

bool URIConfig::set_uri(const std::string& uri) {
    clear();
    std::string tmpuri = uri;
    boost::algorithm::trim_if(tmpuri, boost::is_any_of(" \t\r\n"));
    if (tmpuri.length() == 0)
        return true;
    //params
    auto pos = uri.find("?");
    if (pos != uri.npos) {
        tmpuri = URICode::decode(uri.substr(0, pos));
        std::string params = uri.substr(pos + 1);
        const char* p = params.c_str();
        const char* key = p;
        int key_len = 0;
        const char* val = nullptr;
        int val_len = 0;
        while(true) {
            if (*p == '=') {
                key_len = p - key;
                RCHECK(key_len > 0) << uri << " parse key val fail, key_len == 0.";
                val = ++p;
            } else if (*p == '&' || *p =='\0') {
                RCHECK(key_len > 0) << uri << " parse key val fail, no key.";
                val_len = p - val;

                std::string skey = URICode::decode(std::string(key, key_len));
                std::string sval = URICode::decode(std::string(val, val_len));
                SCHECK(_config.set_val(skey, sval, URILVL::URICFG)) << uri << " param "
                                                                    << skey << " set failed.";

                if (*p == '\0')
                    break;
                key_len = 0;
                val_len = 0;
                key = ++p;
            } else {
                ++p;
            }
        }
    } else {
        tmpuri = URICode::decode(uri);
    }

    //storage
    _store = ShellUtility::fs_type(tmpuri, _prefix, _name);
    if (_store == FileSystemType::UNKNOWN) {
        RLOG(FATAL) << uri << " unknown storage";
    }

    return true;
}

void URIConfig::replace_param(const std::map<std::string,
                                             std::pair<std::string, int>>& params) {
    std::string src = uri();
    for (auto it = params.begin(); it != params.end(); ++it) {
        _config.set_val(it->first, it->second.first, it->second.second);
    }
    SLOG(INFO) << "replace uri [" << src << "] -> [" << uri() << "]";
}

std::string URIConfig::value() const {
    std::string str_param = _config.to_string(URILVL::DEFAULT + 1);
    return _prefix + _name + str_param;
}

std::string ds_config_t::to_string(int low_lvl) const {
    std::ostringstream oss;
    int cnt = 0;
    for (auto it = params.begin(); it != params.end(); ++it) {
        if (it->second.second < low_lvl) {
            continue;
        }
        oss << (cnt++ == 0 ? "?" : "&")
            << it->first << "=" << URICode::encode(it->second.first);
    }
    return oss.str();
}

void DsURIConfig::fill_default() {
    std::string format;
    auto& params_unit = ds_format().params_unit();
    for (auto it_p = params_unit.cbegin(); it_p != params_unit.cend(); ++it_p) {
        if (it_p->second.missing_ok && it_p->second.has_deft) {
            _config.set_val(it_p->first, it_p->second.defval, URILVL::DEFAULT);
        }
    }

    auto& storages = ds_format().storages();
    auto it_s = storages.find(storage_type());
    if (it_s != storages.end()) {
        for (auto& p : it_s->second.params_unit()) {
            if (p.second.missing_ok && p.second.has_deft)
                _config.set_val(p.first, p.second.defval, URILVL::DEFAULT);
        }
    }

    if (!_config.get_val(DS_FORMAT, format))
        return;
    auto& formats = ds_format().formats();
    auto it_f = formats.find(format);
    if (it_f != formats.end()) {
        for (auto& p : it_f->second.params_unit()) {
            if (p.second.missing_ok && p.second.has_deft)
                _config.set_val(p.first, p.second.defval, URILVL::DEFAULT);
        }

    }
}

std::string DsURIConfig::helper_info(const std::string& tab) const {
    std::ostringstream oss;
    oss << tab <<  "  uri: [schema]://path";
    auto& params_unit = ds_format().params_unit();
    int cnt = 0;
    for (auto& p : params_unit) {
        if (p.first == DS_FORMAT) {
            continue;
        }
        if (p.second.has_deft)
            oss << (cnt++ ? "&" : "?") << p.first << "=" << p.second.defval;
    }
    for (auto& p: params_unit) {
        if (p.first == DS_FORMAT)
            continue;
        oss << "\n" << tab << "    " << p.second.helper_info;
    }

    auto& schemas = ds_format().storages();
    if (schemas.size() > 0) {
        oss << "\n" << tab << "  schemas:";
        for (auto& s : schemas) {
            oss << "\n" << tab << "    " << s.second.name() << ": ["  
                << ShellUtility::fs_type_desc(s.first) 
                << "]" << s.second.path_info();
            int cnt = 0;
            for (auto& p : s.second.params_unit()) {
                if (p.second.has_deft)
                    oss << (cnt++ ? "&" : "?") << p.first << "=" << p.second.defval;
            }
            for (auto& p : s.second.params_unit()) {
                oss << "\n" << tab << "      " << p.second.helper_info;
            }
        }
    }

    auto& formats = ds_format().formats();
    if (formats.size() > 0) {
        oss << "\n" << tab << "  formats: ";
        auto it_df = params_unit.find(DS_FORMAT);
        if (it_df != params_unit.end()) {
            oss << it_df->second.helper_info;
        }
        for (auto& f : formats) {
            auto& params_unit = f.second.params_unit();
            oss << "\n" << tab << "    " << f.first << ": (";
            auto& storages = f.second.storage();
            for (std::set<FileSystemType>::iterator it_s = storages.begin();
                 it_s != storages.end(); ++it_s) {
                oss << (it_s == storages.begin() ? "" : "|") << ShellUtility::fs_type_desc(*it_s);
            }
            oss << ")path?format=" << f.first;
            for (auto& p : params_unit) {
                if (p.second.has_deft)
                    oss << "&" << p.first << "=" << p.second.defval;
            }
            for (auto& p : params_unit) {
                oss << "\n" << tab << "      " << p.second.helper_info;
            }
        }
    }

    return oss.str();
}

bool DsURIConfig::load_config(const Configure& config) {
    std::string str;
    SCHECK(config.try_as<std::string>(str));
    if (!set_uri(str))
        return false;
    fill_default();
    return true;
}

bool uri_params_check(const ds_config_t& config, const URIParams& params) {
    for (auto& p : params.params_unit()) {
        std::string val;
        if (!config.get_val(p.first, val)) {
            if (p.second.missing_ok)
                continue;
            SLOG(WARNING) << "uri params missing " << p.first;
            return false;
        }
        if (!p.second.checker(p.first, val)) {
            SLOG(WARNING) << "uri params check " << p.first << "=" << val << " failed";
            return false;
        }
    }
    return true;
}

bool DsURIConfig::check() const {
    if (!valid())
        return true;

    if (!uri_params_check(_config, ds_format()))
        return false;

    FileSystemType fs_type = storage_type();
    auto& storages = ds_format().storages();
    auto it_s = storages.find(fs_type);
    if (it_s == storages.end()) {
        RLOG(WARNING) << uri() << " not support schema " << ShellUtility::fs_type_desc(fs_type);
        return false;
    }
    if (!uri_params_check(_config, it_s->second))
        return false;

    std::string format;
    if (!_config.get_val(DS_FORMAT, format)) {
        return true;
    }
    auto& formats = ds_format().formats();
    auto it_f = formats.find(format);
    if (it_f == formats.end()) {
        RLOG(WARNING) << uri() << " format:" << format << " not support";
        return false;
    }
    if (!uri_params_check(_config, it_f->second))
        return false;

    if (it_f->second.storage().count(fs_type) == 0) {
        RLOG(WARNING) << uri() << " schema:" << ShellUtility::fs_type_desc(_store) << " not support for " << format;
        return false;
    }

    return true;
}

std::string DsURIConfig::value() const {
    if (!valid())
        return "";
    return URIConfig::value();
}

std::string DsURIConfig::value_info(const std::string& tab) const {
    if (!valid())
        return tab;
    return tab + value();
}

PicoJsonNode DsURIConfig::value_info_as_json() const {
    if (!valid())
        return "";
    return value();
}
} // namespace pico
} // namespace paradigm4

