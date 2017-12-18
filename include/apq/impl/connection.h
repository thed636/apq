#pragma once

#include <libpq-fe.h>

#include <apq/asio.h>
#include <boost/algorithm/string/trim.hpp>
#include <string>
#include <sstream>


namespace libapq {
namespace impl {

using pg_conn_handle = std::unique_ptr<PGconn, decltype(&PQfinish)>;

inline bool connection_bad(const pg_conn_handle& handle) noexcept {
    return !handle || PQstatus(handle.get()) == CONNECTION_BAD;
}

inline std::string error_message(const pg_conn_handle& handle) {
    const char* msg = PQerrorMessage(handle.get());
    return msg ? boost::trim_right_copy(std::string(msg)) : std::string{};
}

} // namespace impl
} // namespace libapq
