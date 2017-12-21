#pragma once

#include <apq/connection.h>
#include <apq/detail/bind.h>
#include <libpq-fe.h>

namespace libapq {
namespace impl {

template <typename Handler, typename Connection>
inline void write_poll(Connection& conn, Handler&& h) {
    get_connection_socket(conn).async_write_some(
            asio::null_buffers(), std::forward<Handler>(h));
}

template <typename Handler, typename Connection>
inline void read_poll(Connection& conn, Handler&& h) {
    get_connection_socket(conn).async_read_some(
            asio::null_buffers(), std::forward<Handler>(h));
}

template <typename Connection>
inline decltype(auto) connect_poll(Connection& conn) {
    return PQconnectPoll(get_pg_native_handle(conn));
}

template <typename Handler, typename Connection>
inline error_code start_connection(Connection& conn, const std::string& conninfo) {
    pg_conn_handle handle(PQconnectStart(conninfo.c_str()), PQfinish);
    if (!handle) {
        return done(make_error_code(error::network, "Failed to allocate a new PGconn"));
    }
    set_pg_native_handle(conn, std::move(handle));
    return {};
}

/**
* Asynchronous connection operation
*/
template <typename Handler, typename Context>
struct async_connect_op {
    Context& conn_;
    Handler handler_;

    void done(error_code ec = error_code{}) {
        get_connection_io_context(conn_)
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
    async_connect_op<std::decay_t<Handler>, Connection> {
        conn, std::forward<Handler>(handler)
    }.perform(conninfo);
}

} // namespace impl
} // namespace libapq
