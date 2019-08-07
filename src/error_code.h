#ifndef PARADIGM4_PICO_CORE_ERROR_CODE_H
#define PARADIGM4_PICO_CORE_ERROR_CODE_H

#include <boost/format.hpp>

namespace paradigm4 {
namespace pico {
namespace core {

class ErrorCode {
public:
    static inline std::string to_string(const int base, const int code) {
        if (code >= 0 && code < 1000) {
            auto boost_fmt = boost::format("%07d") % (code + base);
            return boost_fmt.str();
        } else {
            return "BAD_ERRCODE_" + std::to_string(code);
        }
    }
};

enum class CoreErrorCode: int {
    // 0xx: generic
    COMM_FORK_PROC        = 1,
    COMM_ENV_VAR          = 2,

    // 1xx: config related
    CFG_CHECKER           = 101,
    CFG_LOAD              = 102,
    CFG_SAVE              = 103,
    CFG_PARSE             = 104,
    META_SAVE             = 105,
    META_LOAD             = 106,

    // 3xx: filesystem related
    FS_UNKNOWN_TYPE       = 301,
    FS_CREATE_ITEM        = 302,
    FS_ITEM_EXISTS        = 303,
    FS_FILE_OPEN          = 304,
    FS_FILE_CLOSE         = 306,
    FS_PIPE_CLOSE         = 307,
};


#define ERRCODE(base, code) (paradigm4::pico::core::ErrorCode::to_string(base, code))
#define PICO_CORE_ERRCODE(code) (ERRCODE(205000, (int)paradigm4::pico::core::CoreErrorCode::code))

}

} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_PICO_ERROR_CODE_H
