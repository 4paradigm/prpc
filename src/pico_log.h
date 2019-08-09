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
    static Logger& singleton() {
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

class GLogFatalException : public std::exception {
public:
    GLogFatalException(const std::string& msg) : _msg(msg) {}

    const char* what() const noexcept {
        return _msg.c_str();
    }
private:
    std::string _msg;
};

class GLogFatalWrapper {
public:
    explicit GLogFatalWrapper(const char* file_name, int line_number, bool pcheck);

    virtual ~GLogFatalWrapper() noexcept(false);

    std::ostream& stream();

    static void set_fail_func(std::function<void(const std::string&)> func);

    static void fail_func_abort(const std::string& msg);

    static void fail_func_throw(const std::string& msg);

private: 
    static std::function<void(const std::string&)> _fail_func;
    static bool _func_set;

private:
    bool _pcheck = false;
    google::LogMessage* _glog_message;
    google::ErrnoLogMessage* _plog_message; // Workaround: ~ErrnoLogMessage is not virtual
    std::ostringstream _stream;
};


class LogReporter {
public:
    LogReporter()
            : LogReporter("DEBUG", "") {}

    LogReporter(const std::string severity) 
            : LogReporter(severity, "") {}

    LogReporter(const std::string severity, const std::string err_code) 
            : _severity(severity), _err_code(err_code) {}

    virtual ~LogReporter() {}

    LogReporter& operator,(const std::ostream& stream);

    static void initialize();

    static void finalize();

    static void set_log_report_uri(const std::string&);

    static void set_id(const std::string& role, const int32_t rank) {
        _role = role;
        _rank = rank;
        _id = _role + "," + std::to_string(_rank);
        core::Logger::singleton().set_id(_id);
    }

    static std::string get_id() {
        return _id;
    }

    static int32_t get_rank() {
        return _rank;
    }

    static std::string get_role() { 
        return _role;
    }

    static void fail_func() {
        LogReporter::finalize();
        std::abort();
    }


private:
    static int32_t _rank;
    static std::string _role;
    static std::string _id;
    static std::string _log_report_uri;

    static std::string json_encode(const char* value, size_t len);

    static std::string json_encode(const std::string& value) {
        return json_encode(value.c_str(), value.length());
    }

private:
    std::string _severity;
    std::string _err_code;
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

#define P_PLOG_1(severity) \
    PLOG_WRAPPER_ ## severity << '[' << paradigm4::pico::core::Logger::singleton().get_id() << "] "

#define P_PCHECK_1(condition) \
    if (!(condition)) P_PLOG_1(FATAL) << "PCheck failed: " #condition " "


// auxilaries macro, DO NOT USE
#define P_LOG_2(severity, rank) \
    if (paradigm4::pico::core::LogReporter::get_rank() == rank) \
        P_LOG_1(severity)

#define PR_LOG_1(severity) \
    paradigm4::pico::core::LogReporter(#severity), P_LOG_1(severity)
#define PR_LOG_2(severity, rank) \
    if (paradigm4::pico::core::LogReporter::get_rank() == rank) \
        PR_LOG_1(severity)

#define PR_ERR_LOG_2(severity, err_code) \
    paradigm4::pico::core::LogReporter(#severity, err_code), P_LOG_1(severity)
#define PR_ERR_LOG_3(severity, err_code, rank) \
    if (paradigm4::pico::core::LogReporter::get_rank() == rank) \
        PR_ERR_LOG_2(severity, err_code)

#define P_CHECK_2(condition, rank) \
    if (paradigm4::pico::core::LogReporter::get_rank() == rank) \
        P_CHECK_1(condition)

#define P_BLOG_2(level, rank) \
    if (paradigm4::pico::core::LogReporter::get_rank() == rank) \
        P_BLOG_1(level)

#define PR_CHECK_1(condition) \
    if (!(condition)) PR_LOG_1(FATAL) << "Check failed: " #condition " "
#define PR_CHECK_2(condition, rank) \
    if (paradigm4::pico::core::LogReporter::get_rank() == rank) \
        PR_CHECK_1(condition)

#define PR_ERR_CHECK_2(condition, err_code) \
    if (!(condition)) PR_ERR_LOG_2(FATAL, err_code) << "Check failed: " #condition " "
#define PR_ERR_CHECK_3(condition, err_code, rank) \
    if (paradigm4::pico::core::LogReporter::get_rank() == rank) \
        PR_ERR_CHECK_2(condition, err_code)

#define P_PLOG_2(severity, rank) \
    if (paradigm4::pico::core::LogReporter::get_rank() == rank) \
        P_PLOG_1(severity)

#define PR_PLOG_1(severity) \
    paradigm4::pico::core::LogReporter(#severity), P_PLOG_1(severity)
#define PR_PLOG_2(severity, rank) \
    if (paradigm4::pico::core::LogReporter::get_rank() == rank) \
        PR_PLOG_1(severity)

#define PR_ERR_PLOG_2(severity, err_code) \
    paradigm4::pico::core::LogReporter(#severity, err_code), P_PLOG_1(severity)
#define PR_ERR_PLOG_3(severity, err_code, rank) \
    if (paradigm4::pico::core::LogReporter::get_rank() == rank) \
        PR_ERR_PLOG_2(severity, err_code)

#define P_PCHECK_2(condition, rank) \
    if (paradigm4::pico::core::LogReporter::get_rank() == rank) \
        P_PCHECK_1(condition)

#define PR_PCHECK_1(condition) \
    if (!(condition)) PR_PLOG_1(FATAL) << "PCheck failed: " #condition " "
#define PR_PCHECK_2(condition, rank) \
    if (paradigm4::pico::core::LogReporter::get_rank() == rank) \
        PR_PCHECK_1(condition)

#define PR_ERR_PCHECK_2(condition, err_code) \
    if (!(condition)) PR_ERR_PLOG_2(FATAL, err_code) << "PCheck failed: " #condition " "
#define PR_ERR_PCHECK_3(condition, err_code, rank) \
    if (paradigm4::pico::core::LogReporter::get_rank() == rank) \
        PR_ERR_PCHECK_2(condition, err_code)

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

// log to stderr, and report(severity, [rank])
#define RLOG(...) OVERRIDE_MACRO_2(__VA_ARGS__, PR_LOG_2, PR_LOG_1)(__VA_ARGS__)
// log to stderr, and report with errcode(severity, err_code, [rank])
#define ELOG(severity, ...) \
    OVERRIDE_MACRO_2(__VA_ARGS__, PR_ERR_LOG_3, PR_ERR_LOG_2)(severity, __VA_ARGS__)
// ATTENTION, condition will only evaluated on specified rank
#define RCHECK(...) OVERRIDE_MACRO_2(__VA_ARGS__, PR_CHECK_2, PR_CHECK_1)(__VA_ARGS__)
// check with report and errcode(condition, err_code, [rank])
// ATTENTION, condition will only evaluated on specified rank
#define ECHECK(condition, ...) \
    OVERRIDE_MACRO_2(__VA_ARGS, PR_ERR_CHECK_3, PR_ERR_CHECK_2)(condition, __VA_ARGS__)
// plog to stderr, and report(condition, [rank])
#define PRLOG(...) OVERRIDE_MACRO_2(__VA_ARGS__, PR_PLOG_2, PR_PLOG_1)(__VA_ARGS__)
// plog to stderr, and report with errcode(condition, err_code, [rank])
#define PELOG(severity, ...) \
    OVERRIDE_MACRO_2(__VA_ARGS__, PR_ERR_PLOG_3, PR_ERR_PLOG_2)(severity, __VA_ARGS__)
// pcheck with report(condition, [rank])
// ATTENTION, condition will only evaluated on specified rank
#define PRCHECK(...) OVERRIDE_MACRO_2(__VA_ARGS__, PR_PCHECK_2, PR_PCHECK_1)(__VA_ARGS__)
// pcheck with report and errcode(condition, err_code, [rank])
// ATTENTION, condition will only evaluated on specified rank
#define PECHECK(condition, ...) \
    OVERRIDE_MACRO_2(__VA_ARGS__, PR_ERR_PCHECK_3, PR_ERR_PCHECK_2)(condition, __VA_ARGS__)


struct StdErrLog {
    StdErrLog(const char* file, int line) {
        if (_enable)
            std::cerr << basename(file) << ":" << line << " ";

    }

    StdErrLog& stream() {
        return *this;
    }

    ~StdErrLog() {
        if (_enable)
            std::cerr << std::endl;
    }

    template <typename T>
    friend inline StdErrLog& operator<<(StdErrLog& log, T&& t) {
        if (log._enable) {
            std::cerr << std::forward<T>(t);
        }
        return log;
    }

private:
    bool _enable = std::getenv("PICO_ENABLE_STDERR_LOG") != nullptr;

};

#define STDERR_LOG() paradigm4::pico::core::StdErrLog(__FILE__, __LINE__).stream()

} // namespace core
} // namespace pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_CORE_PICO_LOG_H

