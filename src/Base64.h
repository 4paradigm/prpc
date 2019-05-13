#ifndef PARADIGM4_PICO_CORE_BASE64_H
#define PARADIGM4_PICO_CORE_BASE64_H

#include "pico_log.h"

namespace paradigm4 {
namespace pico {
namespace core {

class Base64 {
public:
    static Base64& singleton() {
        static Base64 b;
        return b;
    }

    void base64_encode(const void* input, const size_t in_len, std::string& output){
        SCHECK(input != nullptr) << "invalid input args";
        unsigned char buff[3];
        auto input_char = static_cast<const unsigned char*>(input);
        output.clear();

        for(size_t i = 0; i < in_len / 3; i++) {
            buff[0] = *input_char++;
            buff[1] = *input_char++;
            buff[2] = *input_char++;
            output += encode_table[buff[0] >> 2];
            output += encode_table[((buff[0] << 4) | (buff[1] >> 4)) & 0x3F];
            output += encode_table[((buff[1] << 2) | (buff[2] >> 6)) & 0x3F];
            output += encode_table[buff[2] & 0x3F];
        }
        switch (in_len % 3) {
            case 1:
                buff[0] = *input_char++;
                output += encode_table[(buff[0] & 0xFC) >> 2];
                output += encode_table[((buff[0] & 0x03) << 4)];
                output += "==";
                break;
            case 2:
                buff[0] = *input_char++;
                buff[1] = *input_char++;
                output += encode_table[(buff[0] & 0xFC) >> 2];
                output += encode_table[((buff[0] & 0x03) << 4) | ((buff[1] & 0xF0) >> 4)];
                output += encode_table[((buff[1] & 0x0F) << 2)];
                output += "=";
                break;
        }
    }

    void base64_decode(const std::string& input, void* output, size_t& out_len) {
        SCHECK(output != nullptr) << "invalid input args";
        unsigned int value = 0;
        size_t i= 0;
        size_t in_len = input.length();
        size_t in_idx = 0;
        out_len = 0;
        auto output_char = static_cast<unsigned char*>(output);

        while (i < in_len) {
            SCHECK(is_base64(input[in_idx])) << "invalid base64 char: " << input[in_idx];
            value = decode_table[static_cast<size_t>(input[in_idx++])] << 18;
            value += decode_table[static_cast<size_t>(input[in_idx++])] << 12;
            output_char[out_len++] = static_cast<unsigned char>((value & 0x00FF0000) >> 16);
            if (input[in_idx] != '=') {
                SCHECK(is_base64(input[in_idx])) << "invalid base64 char: " << input[in_idx];
                value += decode_table[static_cast<size_t>(input[in_idx++])] << 6;
                output_char[out_len++] = static_cast<unsigned char>((value & 0x0000FF00) >> 8);
                if (input[in_idx] != '=') {
                    SCHECK(is_base64(input[in_idx])) << "invalid base64 char: " << input[in_idx];
                    value += decode_table[static_cast<size_t>(input[in_idx++])];
                    output_char[out_len++] = static_cast<unsigned char>(value & 0x000000FF);
                }
            }
            i += 4;
         }
        output_char[out_len] = '\0';
    }

private:
    static inline bool is_base64(unsigned char c) {
        return (c == '+') || (c == '/')
            || (c >= '0' && c <= '9')
            || (c >= 'A' && c <= 'Z')
            || (c >= 'a' && c <= 'z');
    }

private:
    Base64() {}

    const char decode_table[123] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            62, // '+'
            0, 0, 0,
            63, // '/'
            52, 53, 54, 55, 56, 57, 58, 59, 60, 61, // '0'-'9'
            0, 0, 0, 0, 0, 0, 0,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
            13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, // 'A'-'Z'
            0, 0, 0, 0, 0, 0,
            26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
            39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 // 'a'-'z'
    };

    const char encode_table[64] = {
            'A','B','C','D','E','F','G','H','I','J',
            'K','L','M','N','O','P','Q','R','S','T',
            'U','V','W','X','Y','Z','a','b','c','d',
            'e','f','g','h','i','j','k','l','m','n',
            'o','p','q','r','s','t','u','v','w','x',
            'y','z','0','1','2','3','4','5','6','7',
            '8','9','+','/' };
};

inline void pico_base64_encode(const void* input, const size_t in_len, std::string& output) {
    Base64::singleton().base64_encode(input, in_len, output);
}

inline void pico_base64_decode(const std::string& input, void* output, size_t& out_len) {
    Base64::singleton().base64_decode(input, output, out_len);
}

} // namespace core
} // namespace pico
} // namespace paradigm4
#endif // PARADIGM4_PICO_CORE_BASE64_H
