#pragma once

#include <libpq-fe.h>

#include <apq/asio.h>
#include <boost/algorithm/string/trim.hpp>
#include <string>
#include <sstream>


namespace libapq {
namespace impl {

using pg_conn_handle = std::unique_ptr<PGconn, decltype(&PQfinish)>;

template <typename OidMap, typename Statistics>
struct connection_context {
    connection_context(io_context& io, Statistics statistics)
    : socket_(io), statistics_(std::move(statistics)) {}

    pg_conn_handle handle_;
    asio::posix::stream_descriptor socket_;
    OidMap oid_map_;
    Statistics statistics_; // statistics metatypes to be defined - counter, duration, whatever?
};

template <typename OidMap, typename Statistics>
inline auto& get_connection_context(
        connection_context<OidMap, Statistics>& ctx) noexcept {
    return ctx;
}

template <typename OidMap, typename Statistics>
inline auto& get_connection_context(
        const connection_context<OidMap, Statistics>& ctx) noexcept {
    return ctx;
}

template <typename OidMap, typename Statistics>
inline decltype(auto) get_native_handle(
        connection_context<OidMap, Statistics>& ctx) noexcept {
    return ctx.handle_.get();
}

template <typename OidMap, typename Statistics>
inline bool connection_bad(connection_context<OidMap, Statistics>& ctx) noexcept {
    decltype(auto) handle = get_native_handle(ctx);
    return !handle || PQstatus(handle) == CONNECTION_BAD;
}

template <typename OidMap, typename Statistics>
inline std::string error_message(connection_context<OidMap, Statistics>& ctx) {
    const char* msg = PQerrorMessage(get_native_handle(ctx));
    return msg ? boost::trim_right_copy(std::string(msg)) : std::string{};
}

template <typename OidMap, typename Statistics>
inline error_code assign_socket(connection_context<OidMap, Statistics>& ctx) {
    int fd = PQsocket(get_native_handle(ctx));
    // if (fd == -1) {
    //     return make_error_code(error::network,
    //         "No server connection is currently open, no PQsocket");
    // }

    int new_fd = dup(fd);
    if (new_fd == -1) {
        using namespace boost::system;
        return make_error_code(error_code{errno, get_posix_category()},
            "dup(fd)");
    }

    error_code ec;
    ctx.socket_.assign(new_fd, ec);

    return ec;
}

template <typename OidMap, typename Statistics>
inline error_code rebind_io_context(connection_context<OidMap, Statistics>& ctx, io_context& io) {
    if (std::addressof(ctx.socket_.get_io_service()) != std::addressof(io)) {
        asio::posix::stream_descriptor s{io};
        error_code ec;
        s.assign(ctx.socket_.native_handle(), ec);
        if (ec) {
            return ec;
        }
        ctx.socket_.release();
        ctx.socket_ = std::move(s);
    }
    return {};
}

} // namespace impl
} // namespace libapq
