#pragma once

#include <apq/async_result.h>
#include <apq/error.h>
#include <apq/type_traits.h>
#include <apq/asio.h>

#include <apq/detail/bind.h>
#include <apq/impl/connection.h>

namespace libapq {

/**
* Function to get connection from provider asynchronously via callback. 
* This is customization point which allows to work with different kinds
* of connection providing. E.g. single connection, get connection from 
* pool, lazy connection, retriable connection and so on. 
*/
template <typename ConnectionProvider, typename Handler>
void async_get_connection(ConnectionProvider&& provider, Handler&& handler) {
    provider(std::forward<Handler>(handler));
}

template <typename ConnectionProvider>
struct get_connection_type {
    using type = typename std::decay_t<ConnectionProvider>::connection_type;
};

template <typename ConnectionProvider>
using connection_type = typename get_connection_type<ConnectionProvider>::type;

using no_statistics = decltype(hana::make_map());

namespace detail {

template <typename T>
decltype(auto) get_connection_context(T&& t) {return std::forward<T>(t);}

}

template <typename ContextHolder>
class connection {
public:
    /**
    * A connection type is provided by the connection as 
    * a ConnectionProvider
    */
    using connection_type = connection;

    connection() = default;
    connection(ContextHolder ctx) : ctx_{std::move(ctx)} {}

    auto& context() noexcept {
        using detail::get_connection_context;
        return get_connection_context(ctx_);
    }
    const auto& context() const noexcept {
        using detail::get_connection_context;
        return get_connection_context(ctx_);
    }

    decltype(auto) get_io_context() noexcept {return context().socket_.get_io_context();}

    /**
    * Returns a reference to statistics of the connection
    */
    auto& statistics() noexcept { return context(ctx_).statistics_;}
    const auto& statistics() const noexcept {return context(ctx_).statistics_;}

    /**
    * Returns true for initialized established and good connection
    */
    operator bool () const noexcept {return !!(*this); }
    bool operator ! () const noexcept { 
        using impl::connection_bad;
        return !(ctx_ && !connection_bad(context())); 
    }

    /**
    * connection can be ConnectionProvider, so it provides itself
    */
    template <typename Handler>
    friend void async_get_connection(connection c, Handler&& h) {
        get_io_context().dispatch(detail::bind(
            std::forward<Handler>(h), error_code{}, std::move(c)));
    }

private:
    ContextHolder ctx_;
};

template <typename Ctx>
inline bool operator == (const connection<Ctx>& l, const connection<Ctx>& r) noexcept {
    return std::addressof(l.context()) == std::addressof(r.context());
}

template <typename Ctx>
inline bool operator != (const connection<Ctx>& l, const connection<Ctx>& r) noexcept {
    return !(l == r);
}

template <typename T, typename Ctx>
inline auto type_oid(const connection<Ctx>& conn) noexcept {
    return type_oid<std::decay_t<T>>(conn.context().oid_map_);
}

template <typename T, typename Ctx>
inline auto type_oid(const connection<Ctx>& conn, const T&) noexcept {
    return type_oid<std::decay_t<T>>(conn);
}

template <typename T, typename Ctx>
inline void set_type_oid(connection<Ctx>& conn, oid_t oid) noexcept {
    set_type_oid<T>(conn.context().oid_map_, oid);
}

/**
* Function to get a connection from provider. 
* Accepts:,
*   provider - connection provider which will be asked for connection
*   token - completion token which determine the continuation of operation
*           it can be callback, yield_context, use_future and other Boost.Asio
*           compatible tokens.
* Returns:
*   completion token depent value like void for callback, connection for the
*   yield_context, std::future<connection> for use_future, and so on.
*/
template <typename ConnectionProvider, typename CompletionToken>
auto get_connection(ConnectionProvider&& provider, CompletionToken&& token) {
    using signature_t = void (error_code, connection_type<ConnectionProvider>);
    async_completion<CompletionToken, signature_t> init(std::forward<CompletionToken>(token));

    async_get_connection(std::forward<ConnectionProvider>(provider), 
            std::move(init.handler));

    return init.result.get();
}

} // namespace libapq
