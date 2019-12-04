#include "pico_lexical_cast.h"

namespace paradigm4 {
namespace pico {
namespace core {

double strtod(const char* str, char** str_end) {
    const static double denorm_min = std::numeric_limits<double>::denorm_min();
    const static double min = std::numeric_limits<double>::min();
    double ret = std::strtod(str, str_end);
    if (errno == ERANGE) {
        // compatitable with glibc 2.12
        //   no ERANGE when value in (-DBL_MIN, -DBL_TRUE_MIN] U [DBL_TRUE_MIN, DBL_MIN)
        if ((ret >= denorm_min && ret < min) || (ret > -min && ret <= -denorm_min)) {
            errno = 0;
        }
    }
    return ret;
}

float strtof(const char* str, char** str_end) {
    const static float denorm_min = std::numeric_limits<float>::denorm_min();
    const static float min = std::numeric_limits<float>::min();
    float ret = std::strtof(str, str_end);
    if (errno == ERANGE) {
        // compatitable with glibc 2.12
        //   no ERANGE when value in (-FLT_MIN, -FLT_TRUE_MIN] U [FLT_TRUE_MIN, FLT_MIN)
        if ((ret >= denorm_min && ret < min) || (ret > -min && ret <= -denorm_min)) {
            errno = 0;
        }
    }
    return ret;
}

} // namespace core
} // namespace pico
} // namespace paradigm4

