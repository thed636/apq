#pragma once

#include <apg/connection.h>
#include <apg/impl/async_connect.h>

#include <chrono>

namespace libapq {

namespace detail {

template <typename Handler, typename Connection>
struct connection_handler {
    Handler handler_;
    Connection conn_;

    void operator() (error_code ec) {
        handler_(std::move(ec), std::move(conn_));
    }

    template <typename Func>
    friend void asio_handler_invoke(Func&& f, connection_handler* ctx) {
        asio_handler_invoke(std::forward<Func>(f), 
            std::addressof(ctx->handler_));
    }
};

} // namespace detail

template <typename OidMap, typename Statistics = no_statistics>
class connection_info {
    io_context& io_;
    std::string raw_;
    Statistics statistics_;

    using connection_ctx = impl::connection<OidMap, Statistics>;
public:
    connection_info(io_context& io, std::string conn_str, Statistics statistics = Statistics{})
    : io_(io), conn_str_(std::move(conn_str)), timeout_(timeout), statistics_(statistics) {}

    std::string to_string() const {return raw_;}

    using connection_type = std::shared_ptr<connection_ctx>;

    io_context& get_io_context() {return io_;}

    template <typename Handler>
    friend void async_get_connection(const connection_info& self, Handler&& h) {
        using handler_type = detail::connection_handler<std::decay_t<Handler>, connection_type>;

        auto conn = std::make_shared<connection_ctx>(
            self.get_io_context(), self.statistics_);

        impl::async_connect(ctx, self.to_string(), 
            handler_type{std::forward<Handler>(h), conn});
    }
};

} // namespace libapq
