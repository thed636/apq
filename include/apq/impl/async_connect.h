#pragma once

#include <apq/connection.h>
#include <apq/detail/bind.h>
#include <libpq-fe.h>

namespace libapq {
namespace impl {

template <typename T, typename Handler, typename = IsConnectiable<T>>
inline void write_poll(T& conn, Handler&& h) {
    get_socket(conn).async_write_some(
            asio::null_buffers(), std::forward<Handler>(h));
}

template <typename T, typename Handler, typename = IsConnectiable<T>>
inline void read_poll(T& conn, Handler&& h) {
    get_socket(conn).async_read_some(
            asio::null_buffers(), std::forward<Handler>(h));
}

template <typename T, typename = IsConnectiable<T>>
inline decltype(auto) connect_poll(T& conn) {
    return PQconnectPoll(get_native_handle(conn));
}

template <typename T, typename = IsConnectiable<T>>
inline error_code start_connection(T& conn, const std::string& conninfo) {
    pg_conn_handle handle(PQconnectStart(conninfo.c_str()), PQfinish);
    if (!handle) {
        return done(make_error_code(error::network, "Failed to allocate a new PGconn"));
    }
    get_handle(conn) = std::move(handle);
    return {};
}

template <typename T, typename = IsConnectiable<T>>
inline error_code assign_socket(T& conn) {
    int fd = PQsocket(get_native_handle(conn));
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
    get_socket(conn).assign(new_fd, ec);

    return ec;
}

/**
* Asynchronous connection operation
*/
template <typename Handler, typename Connection>
struct async_connect_op {
    static_assert(Connectiable<Connection>,
        "Connection type does not meet Connectiable requirements");

    Connection& conn_;
    Handler handler_;

    void done(error_code ec = error_code{}) {
        get_io_context(conn_)
            .post(bind(std::move(handler_), std::move(ec)));
    }

    void perform(const std::string& conninfo) {
        error_code ec = start_connection(conn_, conninfo);
        if (ec) {
            return done(ec);
        }

        if (connection_bad(conn_)) {
            std::ostringstream msg;
            msg << "Connection is bad";
            const auto error = error_message(conn_);
            if (!error.empty()) {
                msg << ": " << error;
            }
            msg << " (" << conninfo << ")";
            return done(make_error_code(error::network, msg.str()));
        }

        ec = assign_socket(conn_);
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

        done(make_error_code(error::network, error_message(conn_.handle())));
    }

    template <typename Func>
    friend void asio_handler_invoke(Func&& f, async_connect_op* ctx) {
        using ::boost::asio::asio_handler_invoke;
        asio_handler_invoke(std::forward<Func>(f), std::addressof(ctx->handler_));
    }
};

template <typename Connection, typename Handler>
inline void async_connect(const std::string& conninfo, Connection& conn,
        Handler&& handler) {
    async_connect_op<std::decay_t<Handler>, std::decay_t<Connection>> {
        conn, std::forward<Handler>(handler)
    }.perform(conninfo);
}

} // namespace impl
} // namespace libapq
