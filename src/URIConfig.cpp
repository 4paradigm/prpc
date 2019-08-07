#include <boost/algorithm/string.hpp>
#include "URIConfig.h"

namespace paradigm4 {
namespace pico {
namespace core {

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

std::string uri_config_t::to_string(int low_lvl) const {
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

}
} // namespace pico
} // namespace paradigm4

