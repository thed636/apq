#pragma once

#include <apq/connection.h>
#include <apq/error.h>
#include <apq/detail/bind.h>
#include <libpq-fe.h>

namespace libapq {
namespace impl {
namespace pq {

template <typename T, typename Handler, typename = Require<Connectiable<T>>>
inline void write_poll(T& conn, Handler&& h) {
    get_socket(conn).async_write_some(
            asio::null_buffers(), std::forward<Handler>(h));
}

template <typename T, typename Handler, typename = Require<Connectiable<T>>>
inline void read_poll(T& conn, Handler&& h) {
    get_socket(conn).async_read_some(
            asio::null_buffers(), std::forward<Handler>(h));
}

template <typename T, typename = Require<Connectiable<T>>>
inline decltype(auto) connect_poll(T& conn) {
    return PQconnectPoll(get_native_handle(conn));
}

template <typename T, typename = Require<Connectiable<T>>>
inline error_code start_connection(T& conn, const std::string& conninfo) {
    pg_conn_handle handle(PQconnectStart(conninfo.c_str()), PQfinish);
    if (!handle) {
        return make_error_code(error::pq_connection_start_failed);
    }
    get_handle(conn) = std::move(handle);
    return {};
}

template <typename T, typename = Require<Connectiable<T>>>
inline error_code assign_socket(T& conn) {
    int fd = PQsocket(get_native_handle(conn));
    if (fd == -1) {
        return make_error_code(error::pq_socket_failed);
    }

    int new_fd = dup(fd);
    if (new_fd == -1) {
        using namespace boost::system;
        return make_error_code(error_code{errno, get_posix_category()},
            "dup(fd)");
    }

    boost::system::error_code ec;
    get_socket(conn).assign(new_fd, ec);

    return make_error_code(ec, "assign socket failed");
}
} // namespace pg

template <typename T, typename = Require<Connectiable<T>>>
inline error_code start_connection(T& conn, const std::string& conninfo) {
    using pq::start_connection;
    return start_connection(unwrap_connection(conn), conninfo);
}

template <typename T, typename = Require<Connectiable<T>>>
inline error_code assign_socket(T& conn) {
    using pq::assign_socket;
    return assign_socket(unwrap_connection(conn));
}

template <typename T, typename Handler, typename = Require<Connectiable<T>>>
inline void write_poll(T& conn, Handler&& h) {
    using pq::write_poll;
    write_poll(unwrap_connection(conn), std::forward<Handler>(h));
}

template <typename T, typename Handler, typename = Require<Connectiable<T>>>
inline void read_poll(T& conn, Handler&& h) {
    using pq::read_poll;
    read_poll(unwrap_connection(conn), std::forward<Handler>(h));
}

template <typename T, typename = Require<Connectiable<T>>>
inline decltype(auto) connect_poll(T& conn) {
    using pq::connect_poll;
    return connect_poll(unwrap_connection(conn));
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
            .post(detail::bind(std::move(handler_), std::move(ec)));
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
            return done(make_error_code(error::pq_connection_status_bad,
                msg.str()));
        }

        ec = assign_socket(conn_);
        if (ec) {
            return done(ec);
        }

        return write_poll(conn_, std::move(*this));
    }

    void operator () (boost::system::error_code ec, std::size_t = 0) {
        if (ec) {
            return done(make_error_code(ec, "error while connection polling"));
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

        done(make_error_code(error::pq_connect_poll_failed,
            error_message(conn_)));
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
