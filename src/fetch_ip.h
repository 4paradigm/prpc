#ifndef PARADIGM4_PICO_CORE_INCLUDE_FETCH_IP_H
#define PARADIGM4_PICO_CORE_INCLUDE_FETCH_IP_H

#include <string>

namespace paradigm4 {
namespace pico {
namespace core {

bool fetch_ip(const std::string& user_set_ip, std::string* ip);

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_WRAPPER_COMMON_INCLUDE_WRAPPER_COMMON_H
