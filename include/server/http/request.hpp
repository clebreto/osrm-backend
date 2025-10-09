#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <boost/asio.hpp>

#include <string>

namespace osrm::server::http
{

struct request
{
    enum class method_type
    {
        GET,
        POST,
        UNKNOWN
    };

    method_type method = method_type::GET;
    std::string uri;
    std::string referrer;
    std::string agent;
    std::string connection;
    std::string content_type;
    std::size_t content_length = 0;
    std::string body;
    boost::asio::ip::address endpoint;
};
} // namespace osrm::server::http

#endif // REQUEST_HPP
