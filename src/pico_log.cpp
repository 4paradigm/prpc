//
// Created by sun on 2018/3/12.
//
#include "pico_log.h"
#include "pico_http.h"  

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


int32_t LogReporter::_rank = -1;
std::string LogReporter::_role = "UNKNOWN";
std::string LogReporter::_id = LogReporter::_role + "," + std::to_string(LogReporter::_rank);

LogReporter &LogReporter::operator,(const std::ostream&stream) {
    bool ret = !_log_report_uri.empty() && http_process(_log_report_uri,
          [&](boost::asio::streambuf& request) {
              std::ostream request_stream(&request);
              std::ostringstream buff;
              buff << "{\"severity\": \"" << json_encode(_severity) << "\", \"msg\": \""
                   << json_encode(((google::LogMessage::LogStream*)&stream)->pbase(),
                            ((google::LogMessage::LogStream*)&stream)->pcount())
                   << "\", \"errcode\": \"" << _err_code << "\"}";
              std::string message = buff.str();
              request_stream << "PUT /RunningInfoReporter/asyncLogging HTTP/1.0\r\n";
              request_stream << "Content-Type: application/json\r\n";
              request_stream << "Content-Length: " << message.length() << "\r\n";
              request_stream << "Connection: close\r\n";
              request_stream << "\r\n";
              request_stream << message;
          },
          [](boost::asio::streambuf&) {});
    if (!ret) {
        LOG(WARNING) << "report log failed";
    }
    return *this;
}

void LogReporter::initialize() {
    bool ret = !_log_report_uri.empty() && http_process(_log_report_uri,
          [&](boost::asio::streambuf& request) {
              std::ostream request_stream(&request);
              request_stream << "GET /RunningInfoReporter/initialize HTTP/1.0\r\n";
              request_stream << "Connection: close\r\n";
              request_stream << "\r\n";
          },
          [](boost::asio::streambuf&) {});
    if (!ret) {
        LOG(WARNING) << "initialize LogReporter failed";
    }
    google::InstallFailureFunction(&LogReporter::fail_func);
}

void LogReporter::finalize() {
    bool ret = !_log_report_uri.empty() && http_process(_log_report_uri,
          [&](boost::asio::streambuf& request) {
              std::ostream request_stream(&request);
              request_stream << "GET /RunningInfoReporter/finalize HTTP/1.0\r\n";
              request_stream << "Connection: close\r\n";
              request_stream << "\r\n";
          },
          [](boost::asio::streambuf&) {});
    if (!ret) {
        LOG(WARNING) << "finalize LogReporter failed";
    }
}

void LogReporter::set_log_report_uri(const std::string& log_report_uri) {
    _log_report_uri = log_report_uri;
}


std::string LogReporter::json_encode(const char* value, size_t len) {
    std::ostringstream escaped;
    for (size_t i = 0; i < len; i++) {
        char c = *(value + i);
        if (c == '"' || c == '\\') {
            escaped << '\\';
        }
        escaped << c;
    }
    return escaped.str();
}

std::string LogReporter::_log_report_uri = "";

} // namespace core
} // namespace pico
} // namespace paradigm4
