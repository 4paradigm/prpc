//
// Created by sun on 2018/4/26.
//
#include <boost/asio.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include "pico_http.h"
namespace paradigm4 {
namespace pico {
namespace core {

/*!
 * \brief send http request
 */
bool http_process(const std::string& host, const std::string& port,
                  std::function<void(boost::asio::streambuf&)> prep_request_func,
                  std::function<void(boost::asio::streambuf&)> proc_response_func) {
    if (host.empty() || port.empty()) {
        return true;
    }
    try {
        // prepare connection
        boost::asio::io_service io_service;
        boost::asio::ip::tcp::resolver resolver(io_service);
        boost::asio::ip::tcp::resolver::query query(host, port);
        boost::asio::ip::tcp::resolver::iterator
            endpoint_iterator = resolver.resolve(query);
        boost::asio::ip::tcp::socket socket(io_service);
        boost::asio::connect(socket, endpoint_iterator);
        // prepare request
        boost::asio::streambuf request;
        prep_request_func(request);
        // send
        boost::asio::write(socket, request);
        // parse response header
        boost::asio::streambuf response;
        boost::asio::read_until(socket, response, "\r\n");
        std::istream response_stream(&response);
        std::string http_version;
        unsigned int status_code;
        std::string status_message;
        response_stream >> http_version >> status_code;
        std::getline(response_stream, status_message);
        if (!(response_stream && http_version.substr(0, 5) == "HTTP/")) {
            return false;
        }
        if (status_code / 100 != 2) {
            return false;
        }
        boost::asio::read_until(socket, response, "\r\n\r\n");
        std::string header;
        while (std::getline(response_stream, header) && header != "\r") {}
        // parse response
        proc_response_func(response);
        boost::system::error_code error;
        while (boost::asio::read(socket, response,
            boost::asio::transfer_at_least(1), error)) {
            proc_response_func(response);
        }
        // check error
        if (error != boost::asio::error::eof) {
            return false;
        }
    } catch (std::exception&) {
        return false;
    }
    return true;
}

bool http_process(const std::string& uri,
                  std::function<void(boost::asio::streambuf&)> prep_request_func,
                  std::function<void(boost::asio::streambuf&)> proc_response_func) {
    if (uri == "") {
        return true;
    }
    std::vector<std::string> token;
    boost::algorithm::split(token, uri, boost::algorithm::is_any_of(":"));
    if (token.size() != 2) {
        return false;
    }
    return http_process(token[0], token[1], prep_request_func, proc_response_func);
}

}
}
}
