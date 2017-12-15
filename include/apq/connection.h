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
void async_get_connection(io_context& io, ConnectionProvider&& provider, Handler&& handler) {
    provider(io, std::forward<Handler>(handler));
}

template <typename ConnectionProvider>
struct get_connection_type {
    using type = typename std::decay_t<ConnectionProvider>::connection_type;
};

template <typename ConnectionProvider>
using connection_type = typename get_connection_type<ConnectionProvider>::type;

using pg_conn_handle = std::unique_ptr<PGconn, decltype(&PQfinish)>;

template <typename OidMap>
class connection_context {
public:
    connection_context(pg_conn_handle&& handle, asio::posix::stream_descriptor&& sock)
    : handle_(std::move(handle)), socket_(std::move(sock)) {}

    OidMap& oid_map() noexcept {return oid_map_;}
    pg_conn_handle& handle() const noexcept {return handle_;}
    asio::posix::stream_descriptor& socket() noexcept {return socket_;}

private:
    pg_conn_handle handle_;
    asio::posix::stream_descriptor socket_;
    OidMap oid_map_;
};


template <typename Ctx>
class basic_connection {
public:
    /**
    * A connection type is provided by the basic_connection as 
    * a ConnectionProvider
    */
    using connection_type = basic_connection;

    basic_connection() = default;
    basic_connection(std::shared_ptr<Ctx>&& ctx)
    : ctx_{std::move(ctx)} {}

    const auto& oid_map() const noexcept { return ctx_->oid_map(); }
    auto& oid_map() noexcept { return ctx_->oid_map(); }
    
    auto handle() const noexcept {
        return ctx_ ? ctx_->handle().get() : nullptr;
    }

    auto& socket() noexcept {return ctx_->socket();}
    const auto& socket() const noexcept {return ctx_->socket();}

    operator bool () const noexcept { return !!(*this); }
    bool operator ! () const noexcept { 
        using impl::connection_bad;
        return !(handle() && !connection_bad(handle())); 
    }

    // basic_connection can be ConnectionProvider, so it provides itself
    template <typename Handler>
    friend void async_get_connection(io_context& io, basic_connection c, Handler&& h) {
        io.dispatch(detail::bind(std::forward<Handler>(h), error_code{}, std::move(c)));
    }
private:
    std::shared_ptr<Ctx> ctx_;
};

template <typename Ctx>
inline bool operator == (const basic_connection<Ctx>& l, const basic_connection<Ctx>& r) noexcept {
    return l.handle() == r.handle();
}

template <typename Ctx>
inline bool operator != (const basic_connection<Ctx>& l, const basic_connection<Ctx>& r) noexcept {
    return l.handle() == r.handle();
}

template <typename T, typename Ctx>
inline auto type_oid(const basic_connection<Ctx>& conn) noexcept {
    return type_oid<std::decay_t<T>>(conn.oid_map());
}

template <typename T, typename Ctx>
inline auto type_oid(const basic_connection<Ctx>& conn, const T&) noexcept {
    return type_oid<std::decay_t<T>>(conn);
}

template <typename T, typename Ctx>
inline void set_type_oid(basic_connection<Ctx>& conn, oid_t oid) noexcept {
    set_type_oid<T>(conn.oid_map(), oid);
}

template <typename OidMap>
using connection = basic_connection<connection_context<OidMap>>;

/**
* Function to get a connection from provider. 
*/
template <typename ConnectionProvider, typename CompletionToken>
auto get_connection(io_context& io, ConnectionProvider&& provider, CompletionToken&& token) {
    detail::async_result_init<
        CompletionToken, 
        void (error_code, connection_type<ConnectionProvider>)
    > init(std::forward<CompletionToken>(token));

    async_get_connection(io, std::forward<ConnectionProvider>(provider), 
            std::move(init.handler));

    return init.result.get();
}

} // namespace libapq
