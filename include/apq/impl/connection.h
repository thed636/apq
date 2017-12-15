#pragma once

#include <libpq-fe.h>

#include <apq/asio.h>
#include <boost/algorithm/string/trim.hpp>
#include <string>
#include <sstream>


namespace libapq {
namespace impl {

inline bool connection_bad(PGconn* handle) noexcept {
    return PQstatus(handle) == CONNECTION_BAD;
}

inline std::string error_message(const PGconn* handle) {
    const char* msg = PQerrorMessage(handle);
    return msg ? boost::trim_right_copy(std::string(msg)) : std::string{};
}

} // namespace impl
} // namespace libapq
