#ifndef PARADIGM4_PICO_CORE_PICO_LOG_H
#define PARADIGM4_PICO_CORE_PICO_LOG_H

#include <iostream>
#include <functional>

#include <glog/logging.h>

namespace paradigm4 {
namespace pico {
namespace core {

class Logger {
public:
    static Logger singleton() {
        static Logger logger;
        return logger;
    }

    void set_id(const std::string& id) {
        _id = id;
    }

    std::string get_id() {
        return _id;
    }

    std::string id() {
        return _id;
    }

private:
    std::string _id;
};

class GLogFatalWrapper {
public:
    explicit GLogFatalWrapper(const char* file_name, int line_number, bool pcheck);

    virtual ~GLogFatalWrapper() noexcept(false);

    std::ostream& stream();

    static void set_fail_func(std::function<void()> func);

    static void fail_func_abort();

    static void fail_func_throw();

private: 
    static std::function<void()> _fail_func;
    static bool _func_set;

private:
    bool _pcheck = false;
    google::LogMessage* _glog_message;
    google::ErrnoLogMessage* _plog_message; // Workaround: ~ErrnoLogMessage is not virtual
};

// auxilaries macro, DO NOT USE
#define OVERRIDE_MACRO_2(_1, _2, FUNC,...) FUNC

#define GLOG_WRAPPER_INFO LOG(INFO)
#define GLOG_WRAPPER_WARNING LOG(WARNING)
#define GLOG_WRAPPER_ERROR LOG(ERROR)
#define GLOG_WRAPPER_FATAL paradigm4::pico::core::GLogFatalWrapper(__FILE__, __LINE__, false).stream()

#define PLOG_WRAPPER_INFO PLOG(INFO)
#define PLOG_WRAPPER_WARNING PLOG(WARNING)
#define PLOG_WRAPPER_ERROR PLOG(ERROR)
#define PLOG_WRAPPER_FATAL paradigm4::pico::core::GLogFatalWrapper(__FILE__, __LINE__, true).stream()

#define P_LOG_1(severity) \
    GLOG_WRAPPER_ ## severity << '[' << paradigm4::pico::core::Logger::singleton().get_id() << "] "

#define P_CHECK_1(condition) \
    if (!(condition)) P_LOG_1(FATAL) << "Check failed: " #condition " "

#define P_BLOG_1(level) \
    VLOG(level) << '[' << paradigm4::pico::core::Logger::singleton().get_id() << "] "

#define PR_CHECK_1(condition) \
    if (!(condition)) PR_LOG_1(FATAL) << "Check failed: " #condition " "

#define P_PLOG_1(severity) \
    GLOG_WRAPPER_ ## severity << '[' << paradigm4::pico::core::Logger::singleton().get_id() << "] "

#define P_PCHECK_1(condition) \
    if (!(condition)) P_PLOG_1(FATAL) << "PCheck failed: " #condition " "

// PICO LOG MACRO

// log to stderr only(severity, [rank])
#define SLOG(...) OVERRIDE_MACRO_2(__VA_ARGS__, P_LOG_2, P_LOG_1)(__VA_ARGS__)
// verbose log, --v=L will only output log which level <= L
#define BLOG(...) OVERRIDE_MACRO_2(__VA_ARGS__, P_BLOG_2, P_BLOG_1)(__VA_ARGS__)
// check(condition, [rank])
// ATTENTION, condition will only evaluated on specified rank
#define SCHECK(...) OVERRIDE_MACRO_2(__VA_ARGS__, P_CHECK_2, P_CHECK_1)(__VA_ARGS__)
// plog to stderr only(condition, [rank])
#define PSLOG(...) OVERRIDE_MACRO_2(__VA_ARGS__, P_PLOG_2, P_PLOG_1)(__VA_ARGS__)
// pcheck(condition, [rank])
// ATTENTION, condition will only evaluated on specified rank
#define PSCHECK(...) OVERRIDE_MACRO_2(__VA_ARGS__, P_PCHECK_2, P_PCHECK_1)(__VA_ARGS__)

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_PICO_LOG_H

