#pragma once

#include <boost/system/system_error.hpp>

namespace libapq {

using error_code = ::boost::system::error_code;
using system_error = ::boost::system::system_error;
using error_category = ::boost::system::error_category;

inline auto make_error_code(::boost::system::error_code code,
        const std::string& /*msg*/ = "") {
    return code;
}

namespace error {

enum code {
    ok, // to do no use error code 0
    pq_connection_start_failed, // PQConnectionStart function failed
    pq_socket_failed, // PQSocket returned -1 as fd - it seems like there is no connection
    pq_connection_status_bad, // PQstatus returned CONNECTION_BAD 
    pq_connect_poll_failed, // PQconnectPoll function failed
};

namespace detail {

class category : public error_category {
public:
    const char* name() const throw() { return "yamail::resource_pool::error::detail::category"; }

    std::string message(int value) const {
        switch (code(value)) {
            case ok:
                return "no error";
            case pq_connection_start_failed:
                return "pq_connection_start_failed - PQConnectionStart function failed";
            case pq_socket_failed:
                return "pq_socket_failed - PQSocket returned -1 as fd - it seems like there is no connection";
            case pq_connection_status_bad:
                return "pq_connection_status_bad - PQstatus returned CONNECTION_BAD";
            case pq_connect_poll_failed:
                return "pq_connect_poll_failed - PQconnectPoll function failed";
        }
        std::ostringstream error;
        error << "no message for value: " << value;
        return error.str();
    }
};

} // namespace detail

inline const error_category& get_category() {
    static detail::category instance;
    return instance;
}

} // namespace error
} // namespace libapq

namespace boost {
namespace system {

template <>
struct is_error_code_enum<libapq::error::code> : std::true_type {};

} // namespace system
} // namespace boost

namespace libapq {
namespace error {

inline auto make_error_code(const code e) {
    return boost::system::error_code(static_cast<int>(e), get_category());
}

} // namespace error
} // namespace libapq
