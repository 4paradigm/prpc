#ifndef PARADIGM4_PICO_COMMON_PICO_HTTP_H
#define PARADIGM4_PICO_COMMON_PICO_HTTP_H

#include <boost/asio/streambuf.hpp>
#include <functional>
#include <string>

namespace paradigm4 {
namespace pico {
namespace core {

/*!
 * \brief send http request
 */
bool http_process(const std::string& host, const std::string& port,
                  std::function<void(boost::asio::streambuf&)> prep_request_func,
                  std::function<void(boost::asio::streambuf&)> proc_response_func);

bool http_process(const std::string& uri,
                  std::function<void(boost::asio::streambuf&)> prep_request_func,
                  std::function<void(boost::asio::streambuf&)> proc_response_func);

}
} // namesapce pico
} // namespace paradigm4

#endif // PARADIGM4_PICO_COMMON_PICO_HTTP_H

