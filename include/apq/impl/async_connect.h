#pragma once

#include <apq/connection.h>
#include <apq/detail/bind.h>
#include <libpq-fe.h>

namespace libapq {
namespace impl {

template <typename OidMap, typename Handler>
inline void write_poll(connection_context<OidMap>& conn, Handler&& h) {
    conn.socket().async_write_some(
            asio::null_buffers(), std::forward<Handler>(h));
}

template <typename OidMap, typename Handler>
inline void read_poll(connection_context<OidMap>& conn, Handler&& h) {
    conn.socket().async_read_some(
            asio::null_buffers(), std::forward<Handler>(h));
}

template <typename OidMap, typename Handler>
inline auto connect_poll(connection_context<OidMap>& conn) {
    return PQconnectPoll(conn.handle().get());
}

inline pg_conn_handle start_connection(const std::string& conninfo) {
    return {PQconnectStart(conninfo.c_str()), PQfinish};
}

inline error_code assign_socket(pg_conn_handle& handle, asio::posix::stream_descriptor& socket) {
    int fd = PQsocket(handle.get());
    if (fd == -1) {
        return make_error_code(error::network,
            "No server connection is currently open, no PQsocket");
    }

    int new_fd = dup(fd);
    if (new_fd == -1) {
        using namespace boost::system;
        return make_error_code(error_code{errno, get_posix_category()},
            "dup(fd)");
    }

    error_code ec;
    socket.assign(new_fd, ec);

    return ec;
}

/**
* Asynchronous connection operation
*/
template <typename IoContext, typename Handler, typename ConnCtx>
struct async_connect_op {
    async_connect_op(IoContext& io, ConnCtx& conn, Handler& handler)
    : io_(io), conn_(conn), handler_(std::move(handler)) {}

    void done(error_code ec = error_code{}) {
        io_.post(bind(std::move(handler_), std::move(ec)));
    }

    void perform(const std::string& conninfo) {
        conn_.handle() = start_connection(conninfo);
        if (!conn_.handle()) {
            return done(make_error_code(error::network, "Failed to allocate a new PGconn"));
        }

        if (connection_bad(conn_.handle())) {
            std::ostringstream msg;
            msg << "Connection is bad";
            const auto error = error_message(conn_.handle());
            if (!error.empty()) {
                msg << ": " << error;
            }
            msg << " (" << conninfo << ")";
            return done(make_error_code(error::network, msg.str()));
        }

        error_code ec = assign_socket(conn_.handle(), conn_.socket());
        if (ec) {
            return done(ec);
        }

        return write_poll(conn_, std::move(*this));
    }

    void operator () (error_code ec, std::size_t = 0) {
        if (ec) {
            return done(ec);
        }

        switch (connect_poll(conn_)) {
        case PGRES_POLLING_OK:
            return done();

        case PGRES_POLLING_WRITING:
            return write_poll(conn_, std::move(*this));

        case PGRES_POLLING_READING:
            return read_poll(conn_, std::move(*this));

        case PGRES_POLLING_FAILED:
        default:
            break;
        }

        done(make_error_code(error::network, error_message(conn_.handle().get())));
    }

    template <typename Func>
    friend void asio_handler_invoke(Func&& f, async_connect_op* ctx) {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(std::forward<Func>(f), std::addressof(ctx->handler_));
    }

    IoContext& io_;
    ConnCtx& conn_;
    Handler handler_;
};

template <typename IoContext, typename Connection, typename Handler>
inline void async_connect(IoContext& io, const std::string& conninfo,
        Connection&& conn, Handler&& handler) {
    async_connect_op<IoContext, std::decay_t<Handler>, std::decay_t<Connection>> {
        io, std::forward<Connection>(conn), std::forward<Handler>(handler)
    }.perform(conninfo);
}

} // namespace impl
} // namespace libapq
