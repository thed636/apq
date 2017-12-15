#pragma once

#include <apg/connection.h>
#include <apg/impl/make_connection.h>

namespace libapq {

namespace detail {

template <typename Handler>
struct construct_connection {
    Handler handler_;

    construct_connection(Handler& handler)
     : handler_(std::move(handler)) {}

    template <typename OidMap>
    void operator() (error_code ec, std::shared_ptr<connection_context<OidMap>>&& impl) {
        handler_(std::move(ec), connection<OidMap>{std::move(impl)});
    }

    template <typename Function>
    inline void asio_handler_invoke(Function&& f, construct_connection* ctx) {
        asio_handler_invoke(std::forward<Function>(f), std::addressof(ctx->handler_));
    }
};

} // namespace detail

template <typename OidMap>
class connection_info {
    std::string raw_;
public:
    connection_info(std::string conn_str)
    : conn_str_(std::move(conn_str)) {}

    std::string to_string() const { return raw_; }

    template <typename Handler>
    friend void async_get_connection(io_context& io, connection_info const& c, Handler&& h) {
        impl::make_connection<OidMap>(io, c.to_string(), 
            construct_connection<std::decay_t<Handler>>{h});
    }
};

} // namespace libapq
