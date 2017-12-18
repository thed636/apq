#pragma once

#include <apg/connection.h>
#include <apg/impl/async_connect.h>

namespace libapq {

namespace detail {

template <typename Handler, typename Context>
struct connection_handler {
    Handler handler_;
    std::shared_ptr<Context> conn_;

    connection_handler(Handler& handler, std::shared_ptr<Context> conn)
     : handler_{std::move(handler)}, conn_{conn} {}

    void operator() (error_code ec) {
        handler_(std::move(ec), basic_connection<Context>{std::move(conn_)});
    }

    template <typename Func>
    inline void asio_handler_invoke(Func&& f, connection_handler* ctx) {
        asio_handler_invoke(std::forward<Function>(f), 
            std::addressof(ctx->handler_));
    }
};

} // namespace detail

template <typename OidMap, typename Statistics = void>
class connection_info {
    std::string raw_;
public:
    connection_info(std::string conn_str)
    : conn_str_(std::move(conn_str)) {}

    std::string to_string() const { return raw_; }

    template <typename Handler>
    friend void async_get_connection(io_context& io, connection_info const& c, Handler&& h) {
        using ctx_type = connection_context<OidMap, Statistics>;
        using handler_type = detail::connection_handler<std::decay_t<Handler>, ctx_type>;

        auto conn = std::make_shared<ctx_type>();
        impl::async_connect(io, *conn, c.to_string(), 
            handler_type{std::forward<Handler>(h), conn});
    }
};

} // namespace libapq
