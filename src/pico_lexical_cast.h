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
T inner_lexical_cast(const S& s) {
    return boost::lexical_cast<T>(s);
}

/*!
 * \brief convert bool value to string
 * \return "true" or "false"
 */
template<>
inline std::string inner_lexical_cast(const bool& b) {
    return b ? "true" : "false";
}

template<>
inline bool inner_lexical_cast(const char* const & s) {
    if (std::strlen(s) == 4 && 
            (std::strncmp(s, "True", 4) == 0 || std::strncmp(s, "true", 4) == 0)) {
        return true;
    }
    if (std::strlen(s) == 5 && 
            (std::strncmp(s, "False", 5) == 0 || std::strncmp(s, "false", 5) == 0)) {
        return false;
    }
    throw std::runtime_error("parse string to bool failed, " + std::string(s));
}

template<>
inline bool inner_lexical_cast(const std::string& s) {
    return inner_lexical_cast<bool>(s.c_str());
}

template<>
inline bool inner_lexical_cast(char* const& s) {
    return inner_lexical_cast<bool, const char*>(s);
}

template<>
inline std::string inner_lexical_cast(const uint8_t& num) {
    return inner_lexical_cast<std::string>(static_cast<uint32_t>(num));
}

template<>
inline std::string inner_lexical_cast(const int8_t& num) {
    return inner_lexical_cast<std::string>(static_cast<int32_t>(num));
}

#define inner_lexical_cast_string_precheck(type, s) \
do { \
    if (s == nullptr) { \
        throw std::runtime_error("parse string to " #type " failed, nullptr"); \
    } \
    if (std::isspace(*s)) { \
        throw std::runtime_error("parse string to " #type " failed, leading whitespace"); \
    } \
    errno = 0; \
} while(0)

#define inner_lexical_cast_string_prechecku(type, s) \
do { \
    inner_lexical_cast_string_precheck(type, s); \
    if (*s == '-') { \
        throw std::runtime_error("parse string to " #type " failed, is negitive"); \
    } \
} while(0)

#define inner_lexical_cast_string_aftcheck(type, s, pos) \
do { \
    if (pos == s) { \
        throw std::runtime_error("parse string to " #type " failed, empty string"); \
    } \
    if (*pos != '\0') { \
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

template<>
inline uint8_t inner_lexical_cast(const char* const& s) {
    inner_lexical_cast_string_prechecku(uint8_t, s);
    char* pos;
    uint32_t ret = std::strtoul(s, &pos, 10);
    inner_lexical_cast_string_aftcheck(uint8_t, s, pos);
    inner_lexical_cast_range_check(uint8_t, ret);
    return ret;
}

template<>
inline uint16_t inner_lexical_cast(const char* const& s) {
    inner_lexical_cast_string_prechecku(uint16_t, s);
    char* pos;
    uint32_t ret = std::strtoul(s, &pos, 10);
    inner_lexical_cast_string_aftcheck(uint16_t, s, pos);
    inner_lexical_cast_range_check(uint16_t, ret);
    return ret;
}

template<>
inline uint32_t inner_lexical_cast(const char* const& s) {
    inner_lexical_cast_string_prechecku(uint32_t, s);
    char* pos;
    uint32_t ret = std::strtoul(s, &pos, 10);
    inner_lexical_cast_string_aftcheck(uint32_t, s, pos);
    return ret;
}

template<>
inline uint64_t inner_lexical_cast(const char* const& s) {
    inner_lexical_cast_string_prechecku(uint64_t, s);
    char* pos;
    uint64_t ret = std::strtoull(s, &pos, 10);
    inner_lexical_cast_string_aftcheck(uint64_t, s, pos);
    return ret;
}

template<>
inline int8_t inner_lexical_cast(const char* const& s) {
    inner_lexical_cast_string_precheck(int8_t, s);
    char* pos;
    int32_t ret = std::strtol(s, &pos, 10);
    inner_lexical_cast_string_aftcheck(int8_t, s, pos);
    inner_lexical_cast_range_check(int8_t, ret);
    return ret;
}

template<>
inline int16_t inner_lexical_cast(const char* const& s) {
    inner_lexical_cast_string_precheck(int16_t, s);
    char* pos;
    int32_t ret = std::strtol(s, &pos, 10);
    inner_lexical_cast_range_check(int16_t, ret);
    inner_lexical_cast_string_aftcheck(int16_t, s, pos);
    return ret;
}

template<>
inline int32_t inner_lexical_cast(const char* const& s) {
    inner_lexical_cast_string_precheck(int32_t, s);
    char* pos;
    int32_t ret = std::strtol(s, &pos, 10);
    inner_lexical_cast_string_aftcheck(int32_t, s, pos);
    return ret;
}

template<>
inline int64_t inner_lexical_cast(const char* const& s) {
    inner_lexical_cast_string_precheck(int64_t, s);
    char* pos;
    int64_t ret = std::strtoll(s, &pos, 10);
    inner_lexical_cast_string_aftcheck(int64_t, s, pos);
    return ret;
}

template<>
inline float inner_lexical_cast(const char* const& s) {
    inner_lexical_cast_string_precheck(float, s);
    char* pos;
    float ret = std::strtof(s, &pos);
    inner_lexical_cast_string_aftcheck(float, s, pos);
    return ret;
}

template<>
inline double inner_lexical_cast(const char* const& s) {
    inner_lexical_cast_string_precheck(double, s);
    char* pos;
    double ret = std::strtod(s, &pos);
    inner_lexical_cast_string_aftcheck(double, s, pos);
    return ret;
}


template<>
inline uint8_t inner_lexical_cast(char* const& s) {
    return inner_lexical_cast<uint8_t, const char*>(s);
}

template<>
inline uint16_t inner_lexical_cast(char* const& s) {
    return inner_lexical_cast<uint16_t, const char*>(s);
}

template<>
inline uint32_t inner_lexical_cast(char* const& s) {
    return inner_lexical_cast<uint32_t, const char*>(s);
}

template<>
inline uint64_t inner_lexical_cast(char* const& s) {
    return inner_lexical_cast<uint64_t, const char*>(s);
}

template<>
inline int8_t inner_lexical_cast(char* const& s) {
    return inner_lexical_cast<int8_t, const char*>(s);
}

template<>
inline int16_t inner_lexical_cast(char* const& s) {
    return inner_lexical_cast<int16_t, const char*>(s);
}

template<>
inline int32_t inner_lexical_cast(char* const& s) {
    return inner_lexical_cast<int32_t, const char*>(s);
}

template<>
inline int64_t inner_lexical_cast(char* const& s) {
    return inner_lexical_cast<int64_t, const char*>(s);
}

template<>
inline float inner_lexical_cast(char* const& s) {
    return inner_lexical_cast<float, const char*>(s);
}

template<>
inline double inner_lexical_cast(char* const& s) {
    return inner_lexical_cast<double, const char*>(s);
}

template<>
inline uint8_t inner_lexical_cast(const std::string& s) {
    return inner_lexical_cast<uint8_t>(s.c_str());
}

template<>
inline uint16_t inner_lexical_cast(const std::string& s) {
    return inner_lexical_cast<uint16_t>(s.c_str());
}

template<>
inline uint32_t inner_lexical_cast(const std::string& s) {
    return inner_lexical_cast<uint32_t>(s.c_str());
}

template<>
inline uint64_t inner_lexical_cast(const std::string& s) {
    return inner_lexical_cast<uint64_t>(s.c_str());
}

template<>
inline int8_t inner_lexical_cast(const std::string& s) {
    return inner_lexical_cast<int8_t>(s.c_str());
}

template<>
inline int16_t inner_lexical_cast(const std::string& s) {
    return inner_lexical_cast<int16_t>(s.c_str());
}

template<>
inline int32_t inner_lexical_cast(const std::string& s) {
    return inner_lexical_cast<int32_t>(s.c_str());
}

template<>
inline int64_t inner_lexical_cast(const std::string& s) {
    return inner_lexical_cast<int64_t>(s.c_str());
}

template<>
inline float inner_lexical_cast(const std::string& s) {
    return inner_lexical_cast<float>(s.c_str());
}

template<>
inline double inner_lexical_cast(const std::string& s) {
    return inner_lexical_cast<double>(s.c_str());
}

/*!
 * \brief convert type S to T. S is char array
 *  use this function need to explicit give output variable type
 * \param s  input variable
 * \return output variable
 */
template<typename T, typename S>
inline std::enable_if_t<std::is_array<std::remove_reference_t<S>>::value &&
        std::is_same<char, std::remove_extent_t<std::remove_reference_t<S>> >::value, T>
pico_lexical_cast(const S& s) {
    return inner_lexical_cast<T, const char*>(s);
}

/*!
 * \brief convert type S to T. S is not array
 *  use this function need to explicit give output variable type
 * \param s  input variable
 * \return output variable
 */
template<typename T, typename S>
inline std::enable_if_t<!std::is_array<std::remove_reference_t<S>>::value, T>
pico_lexical_cast(const S& s) {
    return inner_lexical_cast<T, S>(s);
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
pico_lexical_cast(const S& s) {
    static_assert(std::is_same<char, std::remove_extent_t<std::remove_reference_t<S>> >::value, 
            "only suport char array");
    return T();
}

/*!
 * \brief convert type S to T
 * \param s  input variable
 * \param t  output variable reference
 * \return convert success or not
 */
template<typename T, typename S>
inline bool pico_lexical_cast(const S& s, T& t) {
    try {
        t = pico_lexical_cast<T>(s);
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

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_PICO_LEXICAL_CAST_H
