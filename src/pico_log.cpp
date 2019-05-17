//
// Created by sun on 2018/3/12.
//
#include "pico_log.h"

namespace paradigm4 {
namespace pico {
namespace core {

void GLogFatalWrapper::fail_func_abort(const std::string&) {
    std::abort();
}

void GLogFatalWrapper::fail_func_throw(const std::string& msg) {
    throw GLogFatalException(msg);
}

std::function<void(const std::string&)> GLogFatalWrapper::_fail_func = GLogFatalWrapper::fail_func_abort;

void GLogFatalWrapper::set_fail_func(std::function<void(const std::string&)> func) {
    GLogFatalWrapper::_fail_func = func;
    GLogFatalWrapper::_func_set = true;
}

bool GLogFatalWrapper::_func_set = false;

GLogFatalWrapper::GLogFatalWrapper(const char* file_name, int line_number, bool pcheck)
    : _pcheck(pcheck) {
    if (pcheck) {
        if (GLogFatalWrapper::_func_set) {
            _plog_message = new google::ErrnoLogMessage(file_name,
                  line_number,
                  google::GLOG_ERROR,
                  0,
                  &google::LogMessage::SendToLog);
        } else {
            _plog_message = new google::ErrnoLogMessage(file_name,
                  line_number,
                  google::GLOG_FATAL,
                  0,
                  &google::LogMessage::SendToLog);
        }
    } else {
        if (GLogFatalWrapper::_func_set) {
            _glog_message = new google::LogMessage(file_name, line_number, google::GLOG_ERROR);
        } else {
            _glog_message = new google::LogMessageFatal(file_name, line_number);
        }
    }
    _stream << "[" << file_name << ":" << line_number << "] ";
}

GLogFatalWrapper::~GLogFatalWrapper() noexcept(false) {
    std::string msg = _stream.str();
    if (_pcheck) {
        _plog_message->stream() << msg;
        delete _plog_message;
    } else {
        _glog_message->stream() << msg;
        delete _glog_message;
    }
    if (GLogFatalWrapper::_func_set) {
        GLogFatalWrapper::_fail_func(msg);
    }
}

std::ostream& GLogFatalWrapper::stream() {
    return _stream;
}

} // namespace core
} // namespace pico
} // namespace paradigm4
