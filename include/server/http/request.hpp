#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <boost/asio.hpp>

#include <string>

namespace osrm::server::http
{

struct request
{
    std::string method;      // HTTP method (GET, POST, etc.)
    std::string uri;
    std::string referrer;
    std::string agent;
    std::string connection;
    std::string content_type;
    std::size_t content_length = 0;
    std::string body;        // Request body for POST requests
    boost::asio::ip::address endpoint;
};
} // namespace osrm::server::http

#endif // REQUEST_HPP
