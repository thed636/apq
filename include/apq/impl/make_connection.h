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

/**
* Asynchronous connection operation
*/
template <typename Handler, typename OidMap>
struct async_connect_op {
    async_connect_op(io_context& io, Handler& handler)
    : io_(io), handler_(std::move(handler)) {}

    void done(error_code ec = error_code{}, std::string /*msg*/ = "") {
        io_.post(bind(std::move(handler_), std::move(ec), std::move(conn_)));
    }

    void perform(const std::string& conninfo) {
        pg_conn_handle conn_handle{PQconnectStart(conninfo.c_str()), PQfinish};
        if (!conn_handle) {
            return done(error::network, "Failed to allocate a new PGconn");
        }

        if (connection_bad(conn_handle.get())) {
            std::ostringstream msg;
            msg << "Connection is bad";
            const auto error = error_message(conn_handle.get());
            if (!error.empty()) {
                msg << ": " << error;
            }
            msg << " (" << conninfo << ")";
            return done(error::network, msg.str());
        }

        int fd = PQsocket(conn_handle.get());
        if (fd == -1) {
            return done(error::network,
                "No server connection is currently open, no PQsocket");
        }

        int new_fd = dup(fd);
        if (new_fd == -1) {
            using namespace boost::system;
            return done(error_code{errno, get_posix_category()}, "dup(fd)");
        }

        error_code ec;
        asio::posix::stream_descriptor socket;
        socket.assign(new_fd, ec);
        if (ec) {
            return done(std::move(ec), "assign(new_fd, ec)");
        }

        conn_ = std::make_shared<connection_context<OidMap>>(std::move(conn_handle), std::move(socket));

        return write_poll(*conn_, std::move(*this));
    }

    void operator () (error_code ec, std::size_t = 0) {
        if (ec) {
            return done(ec);
        }

        switch (PQconnectPoll(conn_->handle().get())) {
        case PGRES_POLLING_OK:
            return done();

        case PGRES_POLLING_WRITING:
            return write_poll(*conn_, std::move(*this));

        case PGRES_POLLING_READING:
            return read_poll(*conn_, std::move(*this));

        case PGRES_POLLING_FAILED:
        default:
            break;
        }

        done(error::network, error_message(conn_->handle().get()));
    }

    template <typename Func>
    friend void asio_handler_invoke(Func&& f, async_connect_op* ctx) {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(std::forward<Func>(f), &ctx->handler_);
    }

    io_context& io_;
    std::shared_ptr<connection_context<OidMap>> conn_;
    Handler handler_;
};

template <typename OidMap, typename Handler>
inline void make_connection(io_context& io, const std::string& conninfo, 
        Handler&& handler) {
    async_connect_op<std::decay_t<Handler>, OidMap>(
        impl, std::forward(handler)
    ).perform(conninfo);
}

} // namespace impl
} // namespace libapq
