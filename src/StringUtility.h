#ifndef PARADIGM4_PICO_CORE_STRING_UTILITY_H
#define PARADIGM4_PICO_CORE_STRING_UTILITY_H

#include <string>
#include <cmath>

#include "VirtualObject.h"
#include "pico_log.h"
namespace paradigm4 {
namespace pico {
namespace core {

class StringUtility : public VirtualObject {
public:
    StringUtility() = delete;

    static void split(char* input_str_ptr, 
            std::vector<std::pair<char*, size_t>>& output_token, char delim) {
        SCHECK(delim != '\0') << "deliminator cannot be \\0";
        output_token.clear();
        if (input_str_ptr == nullptr) {
            return;
        }
        size_t token_len = 0;
        char* cur_token = nullptr;
        while (true) {
            cur_token = input_str_ptr;
            while (*input_str_ptr != '\0' && *input_str_ptr != delim) {
                input_str_ptr++;
                token_len++;
            }
            output_token.push_back({cur_token, token_len});
            if (*input_str_ptr == delim) {
                *input_str_ptr = '\0';
                input_str_ptr++;
                token_len = 0;
            } else {
                break;
            }
        }
    }
    
    static void split(std::string& input_str, 
            std::vector<std::pair<char*, size_t>>& output_token, char delim) {
        char* input_str_ptr = const_cast<char*>(input_str.c_str());
        split(input_str_ptr, output_token, delim);
    }
    
    static void split_lines(char* input_str_ptr, 
            std::vector<std::pair<char*, size_t>>& output_token, char delim) {
        SCHECK(delim != '\0') << "deliminator cannot be \\0";
        output_token.clear();
        if (input_str_ptr == nullptr) {
            return;
        }
        size_t token_len = 0;
        char* cur_token = nullptr;
        while (true) {
            cur_token = input_str_ptr;
            while (*input_str_ptr != '\0' && *input_str_ptr != delim && *input_str_ptr != '\n') {
                input_str_ptr++;
                token_len++;
            }
            output_token.push_back({cur_token, token_len});
            if (*input_str_ptr == delim || *input_str_ptr == '\n') {
                *input_str_ptr = '\0';
                input_str_ptr++;
                token_len = 0;
            } else {
                break;
            }
        }
    }
    
    static void split_lines(std::string& input_str, 
            std::vector<std::pair<char*, size_t>>& output_token, char delim) {
        char* input_str_ptr = const_cast<char*>(input_str.c_str());
        split_lines(input_str_ptr, output_token, delim);
    }

#ifndef PICO_DEFINE_TO_STRING_ORIGIN
#define PICO_DEFINE_TO_STRING_ORIGIN(type)                  \
    inline static std::string to_string(type value) {       \
        return std::to_string(value);                       \
    }
#endif

    PICO_DEFINE_TO_STRING_ORIGIN(int);
    PICO_DEFINE_TO_STRING_ORIGIN(long);
    PICO_DEFINE_TO_STRING_ORIGIN(long long);
    PICO_DEFINE_TO_STRING_ORIGIN(unsigned);
    PICO_DEFINE_TO_STRING_ORIGIN(unsigned long);
    PICO_DEFINE_TO_STRING_ORIGIN(unsigned long long);

#ifndef PICO_DEFINE_TO_STRING_NEW
#define PICO_DEFINE_TO_STRING_NEW(type, Fs)                 \
    inline static std::string to_string(type value) {       \
        if (std::isnan(value)) return "nan";                \
        if (value > 1e30) value = 1e30; \
        if (value < -1e30) value = -1e30; \
        char s[60];                                        \
        snprintf(s, 60, Fs, value);                        \
        char *p1 = s, *p2 = s;                              \
        while (*p2 != '.') ++p2;                            \
        p1 = p2 + 1;                                        \
        while (*p1 != '\0') {                               \
            if (*p1 != '0') p2 = p1 + 1;                    \
            ++p1;                                           \
        }                                                   \
        *p2 = '\0';                                         \
        return std::string(s);                              \
    }
#endif

    PICO_DEFINE_TO_STRING_NEW(float, "%.18f");
    PICO_DEFINE_TO_STRING_NEW(double, "%.18lf");
    PICO_DEFINE_TO_STRING_NEW(long double, "%.18LF");
};

} // namespace core
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_CORE_STRING_UTILITY_H
