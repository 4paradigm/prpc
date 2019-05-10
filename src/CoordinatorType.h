#ifndef PARADIGM4_PICO_COMMON_COORDINATOR_TYPE_H
#define PARADIGM4_PICO_COMMON_COORDINATOR_TYPE_H

#include <algorithm>
#include <string>
#include <cstring>

#include "Archive.h"

namespace paradigm4 {
namespace pico {
namespace core {

const char* const COORDINATOR_STR_SYNC = "sync";
const char* const COORDINATOR_STR_ASYNC = "async";

enum CoordinatorType : int8_t {
    SYNC, ASYNC, UNKNOWN
};
PICO_ENUM_SERIALIZATION(CoordinatorType, int8_t);

inline CoordinatorType str_to_coordinator_type(const std::string& str) {
    std::string inner_str = str;
    std::transform(inner_str.begin(), inner_str.end(), inner_str.begin(), ::tolower);

    if (strncmp(inner_str.c_str(), COORDINATOR_STR_SYNC, 5) == 0) {
        return CoordinatorType::SYNC;
    } else if (strncmp(inner_str.c_str(), COORDINATOR_STR_ASYNC, 6) == 0) {
        return CoordinatorType::ASYNC;
    } else {
        return CoordinatorType::UNKNOWN;
    }
}

inline std::string coordinator_type_to_str(const CoordinatorType& type) {
    switch(type) {
        case CoordinatorType::SYNC : return COORDINATOR_STR_SYNC;
        case CoordinatorType::ASYNC : return COORDINATOR_STR_ASYNC;
        default: return "unkown";
    }
}

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_COORDINATOR_TYPE_H
