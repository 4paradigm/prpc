#ifndef PARADIGM4_PICO_CORE_URI_CODE_H
#define PARADIGM4_PICO_CORE_URI_CODE_H

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

#include "pico_log.h"

namespace paradigm4 {
namespace pico {
namespace core {

class URICode {
public:
    static std::string encode(const std::string& uri) {
        std::ostringstream oss;
        for (auto i = uri.begin(); i != uri.end(); ++i) {
            if (isalnum(*i) || *i == '_' || *i == '-' || *i == '.' || *i == '~') {
                oss << *i;
            } else {
                oss << '%' << _hex[*i >> 4] << _hex[*i & 0x0F];
            }
        }
        return oss.str();
    }

    static std::string decode(const std::string& uri) {
        std::ostringstream oss;
        const unsigned char* p = reinterpret_cast<const unsigned char*>(uri.c_str());
        const unsigned char* end = p + uri.length();
        while (p < end) {
            if (*p == '%') {
                SCHECK(end - p > 2) << uri << "uri-decode error";
                unsigned char h = letter_to_hex(*++p);
                unsigned char l = letter_to_hex(*++p);
                ++p;
                oss << (char)(h << 4 | l);
            } else {
                oss << *p++;
            }
        }
        return oss.str();
    }
private:
    static unsigned char letter_to_hex(const unsigned char c) {
        if (c >= 'A' && c <= 'F')
            return c + 10 - 'A';
        else if (c >= 'a' && c <= 'f')
            return c + 10 - 'a';
        else if (c >= '0' && c <= '9')
            return c - '0';
        else {
            SLOG(FATAL) << c << "can not convert to hex"; 
        }
        return '\0';
    }

private:
    constexpr static const char* _hex = "0123456789ABCDEF";
};

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif //PARADIGM4_PICO_CORE_URI_CODE_H
