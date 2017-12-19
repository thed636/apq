#pragma once

#include <apq/connection.h>
#include <apq/detail/bind.h>
#include <libpq-fe.h>

namespace libapq {
namespace impl {

template <typename Handler, typename ConnectionContext>
inline void write_poll(ConnectionContext& conn, Handler&& h) {
    conn.socket_.async_write_some(
            asio::null_buffers(), std::forward<Handler>(h));
}

template <typename Handler, typename ConnectionContext>
inline void read_poll(ConnectionContext& conn, Handler&& h) {
    conn.socket_.async_read_some(
            asio::null_buffers(), std::forward<Handler>(h));
}

template <typename Handler, typename ConnectionContext>
inline decltype(auto) connect_poll(ConnectionContext& conn) {
    return PQconnectPoll(conn.handle_.get());
}

template <typename Handler, typename ConnectionContext>
inline error_code start_connection(ConnectionContext& conn, const std::string& conninfo) {
    conn.handle_.assign(PQconnectStart(conninfo.c_str()), PQfinish);
    if (!conn_.handle_) {
        return done(make_error_code(error::network, "Failed to allocate a new PGconn"));
    }
    return {};
}

/**
* Asynchronous connection operation
*/
template <typename Handler, typename ConnectionContext>
struct async_connect_op {
    ConnectionContext& conn_;
    Handler handler_;

    void done(error_code ec = error_code{}) {
        conn_.get_io_context().post(bind(std::move(handler_), std::move(ec)));
    }

    void perform(const std::string& conninfo) {
        error_code ec = start_connection(conn_, conninfo);
        if (ec) {
            return done(ec);
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

template <typename ConnectionContext, typename Handler>
inline void async_connect(const std::string& conninfo, ConnectionContext& conn,
        Handler&& handler) {
    async_connect_op<std::decay_t<Handler>, ConnectionContext> {
        conn, std::forward<Handler>(handler)
    }.perform(conninfo);
}

} // namespace impl
} // namespace libapq
