#ifndef PARADIGM4_PICO_CORE_PICO_LEXICAL_CAST_H
#define PARADIGM4_PICO_CORE_PICO_LEXICAL_CAST_H


#include <boost/lexical_cast.hpp>
#include <string>
#include <type_traits>
#include "pico_log.h"

namespace paradigm4 {
namespace pico {
namespace core {

template<typename T, typename S>
inline T inner_lexical_cast(const S& s, const size_t /* count */) {
    return boost::lexical_cast<T>(s);
}

template<typename T, typename S>
inline T inner_lexical_cast(const S& s) {
    return inner_lexical_cast<T>(s, 0);
}

// =================== bool related =================================
/*!
 * \brief convert bool value to string
 * \return "true" or "false"
 */
template<>
inline std::string inner_lexical_cast(const bool& b, const size_t count) {
    SCHECK(count == 0);
    return b ? "true" : "false";
}

template<>
inline bool inner_lexical_cast(const char* const & s, const size_t count) {
    size_t actual_len = count > 0 ? count : std::strlen(s);
    if (actual_len == 1) {
        if (std::strncmp(s, "0", 1) == 0) {
            return false;
        } else {
            return true;
        }
    }
    if (actual_len == 4 &&
            (std::strncmp(s, "True", 4) == 0 || std::strncmp(s, "true", 4) == 0)) {
        return true;
    }
    if (actual_len == 5 &&
            (std::strncmp(s, "False", 5) == 0 || std::strncmp(s, "false", 5) == 0)) {
        return false;
    }
    throw std::runtime_error("parse string to bool failed, " + std::string(s));
}

template<>
inline bool inner_lexical_cast(const std::string& s, const size_t count) {
    return inner_lexical_cast<bool>(s.c_str(), count);
}

template<>
inline bool inner_lexical_cast(char* const& s, const size_t count) {
    return inner_lexical_cast<bool, const char*>(s, count);
}
// ==================================================================


// =================== decimal to string ============================
template<>
inline std::string inner_lexical_cast(const uint8_t& num, const size_t count) {
    SCHECK(count == 0);
    return inner_lexical_cast<std::string>(static_cast<uint32_t>(num));
}

template<>
inline std::string inner_lexical_cast(const int8_t& num, const size_t count) {
    SCHECK(count == 0);
    return inner_lexical_cast<std::string>(static_cast<int32_t>(num));
}
// ==================================================================



// ========= string/char* to decimal ====================
#define inner_lexical_cast_string_precheck(type, s, count) \
do { \
    if (s == nullptr) { \
        throw std::runtime_error("parse string to " #type " failed, nullptr"); \
    } \
    if (std::isspace(*s)) { \
        throw std::runtime_error("parse string to " #type " failed, leading whitespace"); \
    } \
    if (count > 0 && std::strlen(s) < count) { \
        throw std::runtime_error("parse string to " #type " failed, strlen too short"); \
    } \
    errno = 0; \
} while(0)

#define inner_lexical_cast_string_prechecku(type, s, count) \
do { \
    inner_lexical_cast_string_precheck(type, s, count); \
    if (*s == '-') { \
        throw std::runtime_error("parse string to " #type " failed, is negitive"); \
    } \
} while(0)

#define inner_lexical_cast_string_aftcheck(type, s, pos, count) \
do { \
    if (pos == s) { \
        throw std::runtime_error("parse string to " #type " failed, empty string"); \
    } \
    if (pos < s) { \
        throw std::runtime_error("parse string to " #type " failed, end ptr befor the start"); \
    } \
    if ((count > 0 && (size_t)(pos - s) != count) || (count == 0 && *pos != '\0')) { \
        throw std::runtime_error("parse string to " #type " failed, unused char"); \
    } \
    if (errno == ERANGE) { \
        errno = 0; \
        throw std::runtime_error("parse string to " #type " failed, out of range"); \
    } \
} while(0)

#define inner_lexical_cast_range_check(type, num) \
do { \
    if (num < std::numeric_limits<type>::min() || \
            num > std::numeric_limits<type>::max()) { \
        throw std::runtime_error("parse string to " #type " failed, out of range"); \
    } \
} while(0)

#ifdef __GNUC__
#if __GUNC__ >= 7
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif
#endif
template<typename T>
inline T strntox(T (&func)(const char*, char**, int), 
            const char* str, char** endptr, int base, size_t num) {
    if (str != nullptr && num > 0 && !isspace(str[num]) && str[num] != '\0') {
        std::string new_str(str, num);
        char* new_endptr;
        T ret = func(new_str.c_str(), &new_endptr, base);
        *endptr = (char *)(str + (size_t)(new_endptr - new_str.c_str()));
        return ret;
    } else {
        return func(str, endptr, base);
    }
}

template<typename T>
inline T strntox(T (&func)(const char*, char**), 
            const char* str, char** endptr, size_t num) {
    if (str != nullptr && num > 0 && !isspace(str[num]) && str[num] != '\0') {
        std::string new_str(str, num);
        char* new_endptr;
        T ret = func(new_str.c_str(), &new_endptr);
        *endptr = (char *)(str + (size_t)(new_endptr - new_str.c_str()));
        return ret;
    } else {
        return func(str, endptr);
    }
}
#ifdef __GNUC__
#if __GNUC__ >= 7
#pragma GCC diagnostic pop
#endif
#endif

template<>
inline uint8_t inner_lexical_cast(const char* const& s, const size_t count) {
    inner_lexical_cast_string_prechecku(uint8_t, s, count);
    char* pos;
    uint32_t ret = strntox(std::strtoul, s, &pos, 10, count);
    inner_lexical_cast_string_aftcheck(uint8_t, s, pos, count);
    inner_lexical_cast_range_check(uint8_t, ret);
    return ret;
}

template<>
inline uint16_t inner_lexical_cast(const char* const& s, const size_t count) {
    inner_lexical_cast_string_prechecku(uint16_t, s, count);
    char* pos;
    uint32_t ret = strntox(std::strtoul, s, &pos, 10, count);
    inner_lexical_cast_string_aftcheck(uint16_t, s, pos, count);
    inner_lexical_cast_range_check(uint16_t, ret);
    return ret;
}

template<>
inline uint32_t inner_lexical_cast(const char* const& s, const size_t count) {
    inner_lexical_cast_string_prechecku(uint32_t, s, count);
    char* pos;
    uint32_t ret = strntox(std::strtoul, s, &pos, 10, count);
    inner_lexical_cast_string_aftcheck(uint32_t, s, pos, count);
    return ret;
}

template<>
inline uint64_t inner_lexical_cast(const char* const& s, const size_t count) {
    inner_lexical_cast_string_prechecku(uint64_t, s, count);
    char* pos;
    uint64_t ret = strntox(std::strtoull, s, &pos, 10, count);
    inner_lexical_cast_string_aftcheck(uint64_t, s, pos, count);
    return ret;
}

template<>
inline int8_t inner_lexical_cast(const char* const& s, const size_t count) {
    inner_lexical_cast_string_precheck(int8_t, s, count);
    char* pos;
    int32_t ret = strntox(std::strtol, s, &pos, 10, count);
    inner_lexical_cast_string_aftcheck(int8_t, s, pos, count);
    inner_lexical_cast_range_check(int8_t, ret);
    return ret;
}

template<>
inline int16_t inner_lexical_cast(const char* const& s, const size_t count) {
    inner_lexical_cast_string_precheck(int16_t, s, count);
    char* pos;
    int32_t ret = strntox(std::strtol, s, &pos, 10, count);
    inner_lexical_cast_range_check(int16_t, ret);
    inner_lexical_cast_string_aftcheck(int16_t, s, pos, count);
    return ret;
}

template<>
inline int32_t inner_lexical_cast(const char* const& s, const size_t count) {
    inner_lexical_cast_string_precheck(int32_t, s, count);
    char* pos;
    int32_t ret = strntox(std::strtol, s, &pos, 10, count);
    inner_lexical_cast_string_aftcheck(int32_t, s, pos, count);
    return ret;
}

template<>
inline int64_t inner_lexical_cast(const char* const& s, const size_t count) {
    inner_lexical_cast_string_precheck(int64_t, s, count);
    char* pos;
    int64_t ret = strntox(std::strtoll, s, &pos, 10, count);
    inner_lexical_cast_string_aftcheck(int64_t, s, pos, count);
    return ret;
}

template<>
inline float inner_lexical_cast(const char* const& s, const size_t count) {
    inner_lexical_cast_string_precheck(float, s, count);
    char* pos;
    float ret = strntox(std::strtof, s, &pos, count);
    inner_lexical_cast_string_aftcheck(float, s, pos, count);
    return ret;
}

template<>
inline double inner_lexical_cast(const char* const& s, const size_t count) {
    inner_lexical_cast_string_precheck(double, s, count);
    char* pos;
    double ret = strntox(std::strtod, s, &pos, count);
    inner_lexical_cast_string_aftcheck(double, s, pos, count);
    return ret;
}

template<>
inline uint8_t inner_lexical_cast(char* const& s, const size_t count) {
    return inner_lexical_cast<uint8_t, const char*>(s, count);
}

template<>
inline uint16_t inner_lexical_cast(char* const& s, const size_t count) {
    return inner_lexical_cast<uint16_t, const char*>(s, count);
}

template<>
inline uint32_t inner_lexical_cast(char* const& s, const size_t count) {
    return inner_lexical_cast<uint32_t, const char*>(s, count);
}

template<>
inline uint64_t inner_lexical_cast(char* const& s, const size_t count) {
    return inner_lexical_cast<uint64_t, const char*>(s, count);
}

template<>
inline int8_t inner_lexical_cast(char* const& s, const size_t count) {
    return inner_lexical_cast<int8_t, const char*>(s, count);
}

template<>
inline int16_t inner_lexical_cast(char* const& s, const size_t count) {
    return inner_lexical_cast<int16_t, const char*>(s, count);
}

template<>
inline int32_t inner_lexical_cast(char* const& s, const size_t count) {
    return inner_lexical_cast<int32_t, const char*>(s, count);
}

template<>
inline int64_t inner_lexical_cast(char* const& s, const size_t count) {
    return inner_lexical_cast<int64_t, const char*>(s, count);
}

template<>
inline float inner_lexical_cast(char* const& s, const size_t count) {
    return inner_lexical_cast<float, const char*>(s, count);
}

template<>
inline double inner_lexical_cast(char* const& s, const size_t count) {
    return inner_lexical_cast<double, const char*>(s, count);
}

template<>
inline uint8_t inner_lexical_cast(const std::string& s, const size_t count) {
    return inner_lexical_cast<uint8_t>(s.c_str(), count);
}

template<>
inline uint16_t inner_lexical_cast(const std::string& s, const size_t count) {
    return inner_lexical_cast<uint16_t>(s.c_str(), count);
}

template<>
inline uint32_t inner_lexical_cast(const std::string& s, const size_t count) {
    return inner_lexical_cast<uint32_t>(s.c_str(), count);
}

template<>
inline uint64_t inner_lexical_cast(const std::string& s, const size_t count) {
    return inner_lexical_cast<uint64_t>(s.c_str(), count);
}

template<>
inline int8_t inner_lexical_cast(const std::string& s, const size_t count) {
    return inner_lexical_cast<int8_t>(s.c_str(), count);
}

template<>
inline int16_t inner_lexical_cast(const std::string& s, const size_t count) {
    return inner_lexical_cast<int16_t>(s.c_str(), count);
}

template<>
inline int32_t inner_lexical_cast(const std::string& s, const size_t count) {
    return inner_lexical_cast<int32_t>(s.c_str(), count);
}

template<>
inline int64_t inner_lexical_cast(const std::string& s, const size_t count) {
    return inner_lexical_cast<int64_t>(s.c_str(), count);
}

template<>
inline float inner_lexical_cast(const std::string& s, const size_t count) {
    return inner_lexical_cast<float>(s.c_str(), count);
}

template<>
inline double inner_lexical_cast(const std::string& s, const size_t count) {
    return inner_lexical_cast<double>(s.c_str(), count);
}
// ==================================================================

/*!
 * \brief convert type S to T. S is char array
 *  use this function need to explicit give output variable type
 * \param s  input variable
 * \return output variable
 */
template<typename T, typename S>
inline std::enable_if_t<std::is_array<std::remove_reference_t<S>>::value &&
        std::is_same<char, std::remove_extent_t<std::remove_reference_t<S>> >::value, T>
pico_lexical_cast(const S& s, const size_t count = 0) {
    return inner_lexical_cast<T, const char*>(s, count);
}

/*!
 * \brief convert type S to T. S is not array
 *  use this function need to explicit give output variable type
 * \param s  input variable
 * \return output variable
 */
template<typename T, typename S>
inline std::enable_if_t<!std::is_array<std::remove_reference_t<S>>::value, T>
pico_lexical_cast(const S& s, const size_t count = 0) {
    return inner_lexical_cast<T, S>(s, count);
}

/*!
 * \brief convert type S to T. S is array, except char array, not implement yet.
 *  use this function need to explicit give output variable type
 * \param s  input variable
 * \return output variable
 */
template<typename T, typename S>
inline std::enable_if_t<std::is_array<std::remove_reference_t<S>>::value &&
        !std::is_same<char, std::remove_extent_t<std::remove_reference_t<S>> >::value, T>
pico_lexical_cast(const S& s, const size_t count = 0) {
    static_assert(std::is_same<char, std::remove_extent_t<std::remove_reference_t<S>> >::value, 
            "only suport char array");
    SLOG(FATAL) << "not implemented";
    return T();
}

/*!
 * \brief convert type S to T
 * \param s  input variable
 * \param t  output variable reference
 * \return convert success or not
 */
template<typename T, typename S>
inline bool pico_lexical_cast(const S& s, T& t, const size_t count = 0) {
    try {
        t = pico_lexical_cast<T>(s, count);
        return true;
    } catch (boost::bad_lexical_cast& e) {
        SLOG(WARNING) << "boost lexical_cast exception: " << e.what();
    } catch (std::exception& e) {
        SLOG(WARNING) << "lexical_cast exception:" << e.what();
    } catch (...) {
        SLOG(WARNING) << "lexical_cast unkown exception";
    }
    return false;
}

template<typename T, typename S>
inline T pico_lexical_cast_check(const S& s, const size_t count = 0) {
    T t = T();
    SCHECK((pico_lexical_cast<T, S>(s, t, count))) 
        << "pico_lexical_cast failed, s=" << s << ", count=" << count;
    return t;
}

template<typename T>
inline std::string decimal_to_hex(const T& s) {
    static_assert(std::is_same<T, double>::value
               || std::is_same<T, float>::value 
               || std::is_same<T, long double>::value, 
               "not implemented");
    return std::string();
}

template<>
inline std::string decimal_to_hex(const double& s) {
    const static int SIZE = 64;
    char buf[SIZE];
    int len = snprintf(buf, SIZE, "%la", s);
    if (len >= 0 && len < SIZE && buf[len] == '\0') {
        return std::string(buf);
    }
    throw std::runtime_error("buffer overflow while convert decimal to hex-string. len="
            + std::to_string(len));
    return std::string();
}

template<>
inline std::string decimal_to_hex(const float& s) {
    const static int SIZE = 48;
    char buf[SIZE];
    int len = snprintf(buf, 48, "%a", s);
    if (len >= 0 && len < SIZE && buf[len] == '\0') {
        return std::string(buf);
    }
    throw std::runtime_error("buffer overflow while convert decimal to hex-string. len="
            + std::to_string(len));
    return std::string();
}

template<>
inline std::string decimal_to_hex(const long double& s) {
    const static int SIZE = 128;
    char buf[SIZE];
    int len = snprintf(buf, 128, "%La", s);
    if (len >= 0 && len < SIZE && buf[len] == '\0') {
        return std::string(buf);
    }
    throw std::runtime_error("buffer overflow while convert decimal to hex-string. len="
            + std::to_string(len));
    return std::string();
}

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_PICO_LEXICAL_CAST_H
