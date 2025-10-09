#include "server/request_parser.hpp"

#include "server/http/compression_type.hpp"
#include "server/http/header.hpp"
#include "server/http/request.hpp"

#include <boost/algorithm/string/predicate.hpp>

namespace osrm::server
{

RequestParser::RequestParser()
    : state(internal_state::method_start), current_header({"", ""}),
      selected_compression(http::no_compression), method_string()
{
}

std::tuple<RequestParser::RequestStatus, http::compression_type>
RequestParser::parse(http::request &current_request, char *begin, char *end)
{
    while (begin != end)
    {
        RequestStatus result = consume(current_request, *begin++);
        if (result != RequestStatus::indeterminate)
        {
            return std::make_tuple(result, selected_compression);
        }
    }
    RequestStatus result = RequestStatus::indeterminate;

    return std::make_tuple(result, selected_compression);
}

RequestParser::RequestStatus RequestParser::consume(http::request &current_request,
                                                    const char input)
{
    switch (state)
    {
    case internal_state::method_start:
        if (!is_char(input) || is_CTL(input) || is_special(input))
        {
            return RequestStatus::invalid;
        }
        method_string.clear();
        method_string.push_back(input);
        state = internal_state::method;
        return RequestStatus::indeterminate;
    case internal_state::method:
        if (input == ' ')
        {
            // Determine method type
            if (method_string == "GET")
            {
                current_request.method = http::request::method_type::GET;
            }
            else if (method_string == "POST")
            {
                current_request.method = http::request::method_type::POST;
            }
            else
            {
                current_request.method = http::request::method_type::UNKNOWN;
            }
            state = internal_state::uri;
            return RequestStatus::indeterminate;
        }
        if (!is_char(input) || is_CTL(input) || is_special(input))
        {
            return RequestStatus::invalid;
        }
        method_string.push_back(input);
        return RequestStatus::indeterminate;
    case internal_state::uri_start:
        if (is_CTL(input))
        {
            return RequestStatus::invalid;
        }
        state = internal_state::uri;
        current_request.uri.push_back(input);
        return RequestStatus::indeterminate;
    case internal_state::uri:
        if (input == ' ')
        {
            state = internal_state::http_version_h;
            return RequestStatus::indeterminate;
        }
        if (is_CTL(input))
        {
            return RequestStatus::invalid;
        }
        current_request.uri.push_back(input);
        return RequestStatus::indeterminate;
    case internal_state::http_version_h:
        if (input == 'H')
        {
            state = internal_state::http_version_t_1;
            return RequestStatus::indeterminate;
        }
        return RequestStatus::invalid;
    case internal_state::http_version_t_1:
        if (input == 'T')
        {
            state = internal_state::http_version_t_2;
            return RequestStatus::indeterminate;
        }
        return RequestStatus::invalid;
    case internal_state::http_version_t_2:
        if (input == 'T')
        {
            state = internal_state::http_version_p;
            return RequestStatus::indeterminate;
        }
        return RequestStatus::invalid;
    case internal_state::http_version_p:
        if (input == 'P')
        {
            state = internal_state::http_version_slash;
            return RequestStatus::indeterminate;
        }
        return RequestStatus::invalid;
    case internal_state::http_version_slash:
        if (input == '/')
        {
            state = internal_state::http_version_major_start;
            return RequestStatus::indeterminate;
        }
        return RequestStatus::invalid;
    case internal_state::http_version_major_start:
        if (is_digit(input))
        {
            state = internal_state::http_version_major;
            return RequestStatus::indeterminate;
        }
        return RequestStatus::invalid;
    case internal_state::http_version_major:
        if (input == '.')
        {
            state = internal_state::http_version_minor_start;
            return RequestStatus::indeterminate;
        }
        if (is_digit(input))
        {
            return RequestStatus::indeterminate;
        }
        return RequestStatus::invalid;
    case internal_state::http_version_minor_start:
        if (is_digit(input))
        {
            state = internal_state::http_version_minor;
            return RequestStatus::indeterminate;
        }
        return RequestStatus::invalid;
    case internal_state::http_version_minor:
        if (input == '\r')
        {
            state = internal_state::expecting_newline_1;
            return RequestStatus::indeterminate;
        }
        if (is_digit(input))
        {
            return RequestStatus::indeterminate;
        }
        return RequestStatus::invalid;
    case internal_state::expecting_newline_1:
        if (input == '\n')
        {
            state = internal_state::header_line_start;
            return RequestStatus::indeterminate;
        }
        return RequestStatus::invalid;
    case internal_state::header_line_start:
        if (boost::iequals(current_header.name, "Accept-Encoding"))
        {
            /* giving gzip precedence over deflate */
            if (boost::icontains(current_header.value, "deflate"))
            {
                selected_compression = http::deflate_rfc1951;
            }
            if (boost::icontains(current_header.value, "gzip"))
            {
                selected_compression = http::gzip_rfc1952;
            }
        }

        if (boost::iequals(current_header.name, "Referer"))
        {
            current_request.referrer = current_header.value;
        }

        if (boost::iequals(current_header.name, "User-Agent"))
        {
            current_request.agent = current_header.value;
        }

        if (boost::iequals(current_header.name, "Connection"))
        {
            current_request.connection = current_header.value;
        }

        if (boost::iequals(current_header.name, "Content-Type"))
        {
            current_request.content_type = current_header.value;
        }

        if (boost::iequals(current_header.name, "Content-Length"))
        {
            try
            {
                current_request.content_length = std::stoull(current_header.value);
            }
            catch (...)
            {
                return RequestStatus::invalid;
            }
        }

        if (input == '\r')
        {
            state = internal_state::expecting_newline_3;
            return RequestStatus::indeterminate;
        }
        if (!is_char(input) || is_CTL(input) || is_special(input))
        {
            return RequestStatus::invalid;
        }
        state = internal_state::header_name;
        current_header.clear();
        current_header.name.push_back(input);
        return RequestStatus::indeterminate;
    case internal_state::header_lws:
        if (input == '\r')
        {
            state = internal_state::expecting_newline_2;
            return RequestStatus::indeterminate;
        }
        if (input == ' ' || input == '\t')
        {
            return RequestStatus::indeterminate;
        }
        if (is_CTL(input))
        {
            return RequestStatus::invalid;
        }
        state = internal_state::header_value;
        return RequestStatus::indeterminate;
    case internal_state::header_name:
        if (input == ':')
        {
            state = internal_state::header_value;
            return RequestStatus::indeterminate;
        }
        if (!is_char(input) || is_CTL(input) || is_special(input))
        {
            return RequestStatus::invalid;
        }
        current_header.name.push_back(input);
        return RequestStatus::indeterminate;
    case internal_state::header_value:
        if (input == ' ')
        {
            state = internal_state::header_value;
            return RequestStatus::indeterminate;
        }
        if (input == '\r')
        {
            state = internal_state::expecting_newline_2;
            return RequestStatus::indeterminate;
        }
        if (is_CTL(input))
        {
            return RequestStatus::invalid;
        }
        current_header.value.push_back(input);
        return RequestStatus::indeterminate;
    case internal_state::expecting_newline_2:
        if (input == '\n')
        {
            state = internal_state::header_line_start;
            return RequestStatus::indeterminate;
        }
        return RequestStatus::invalid;
    case internal_state::expecting_newline_3:
        if (input != '\n')
        {
            return RequestStatus::invalid;
        }
        // Headers complete - check if we need to read body
        if (current_request.method == http::request::method_type::POST &&
            current_request.content_length > 0)
        {
            // Reserve space for body and switch to body reading state
            current_request.body.reserve(current_request.content_length);
            state = internal_state::body_reading;
            return RequestStatus::indeterminate;
        }
        // No body expected, request is complete
        return RequestStatus::valid;
    case internal_state::body_reading:
        // Accumulate body data
        current_request.body.push_back(input);
        if (current_request.body.size() >= current_request.content_length)
        {
            // Body complete
            return RequestStatus::valid;
        }
        return RequestStatus::indeterminate;
    default:
        return RequestStatus::invalid;
    }
}

bool RequestParser::is_char(const int character) const
{
    return character >= 0 && character <= 127;
}

bool RequestParser::is_CTL(const int character) const
{
    return (character >= 0 && character <= 31) || (character == 127);
}

bool RequestParser::is_special(const int character) const
{
    switch (character)
    {
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
    case '{':
    case '}':
    case ' ':
    case '\t':
        return true;
    default:
        return false;
    }
}

bool RequestParser::is_digit(const int character) const
{
    return character >= '0' && character <= '9';
}
} // namespace osrm::server
