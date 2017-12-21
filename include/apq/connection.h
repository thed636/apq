#pragma once

#include <apq/async_result.h>
#include <apq/error.h>
#include <apq/type_traits.h>
#include <apq/asio.h>

#include <apq/detail/bind.h>
#include <apq/impl/connection.h>

namespace libapq {

using impl::pg_conn_handle;

template <typename ConnectionProvider, typename Enable = void>
struct get_connection_type {
    using type = typename std::decay_t<ConnectionProvider>::connection_type;
};

template <typename ConnectionProvider>
using connection_type = typename get_connection_type<ConnectionProvider>::type;

using no_statistics = decltype(hana::make_map());

/**
* Marker to tag a connection type.
*/
template <typename, typename = std::void_t<>>
struct is_connection : std::false_type {};

template <typename T>
struct is_connection<T, std::void_t<
    decltype(get_connection_oid_map(std::declval<T&>())),
    decltype(get_connection_socket(std::declval<T&>())),
    decltype(get_connection_handle(std::declval<T&>()))
>> : std::true_type {};

template <typename, typename = std::void_t<>>
struct is_connection_provider_functor : std::false_type {};
template <typename T>
struct is_connection_provider_functor<T, std::void_t<
    decltype( std::declval<T>() (std::declval<void(error_code, connection_type<T>)>()) )
>> : std::true_type {};

template <typename, typename = std::void_t<>>
struct is_connection_wrapper : std::false_type {};

template <typename T>
struct is_connection_wrapper<::std::shared_ptr<T>> : is_connection<T> {};
template <typename T, typename D>
struct is_connection_wrapper<::std::unique_ptr<T, D>> : is_connection<T> {};
template <typename T>
struct is_connection_wrapper<::std::optional<T>> : is_connection<T> {};
template <typename T>
struct is_connection_wrapper<::boost::shared_ptr<T>> : is_connection<T> {};
template <typename T>
struct is_connection_wrapper<::boost::scoped_ptr<T>> : is_connection<T> {};
template <typename T>
struct is_connection_wrapper<::boost::optional<T>> : is_connection<T> {};

template <typename T>
constexpr auto ConnectionWrapper = is_connection_wrapper<std::decay_t<T>>::value;

template <typename T>
constexpr auto Connection = is_connection<std::decay_t<T>>::value;

template <typename T>
constexpr auto Connectiable = Connection<T> || ConnectionWrapper<T>;

template <typename T>
constexpr auto ConnectionProviderFunctor = is_connection_provider_functor<std::decay_t<T>>::value;

template <bool Condition, typename Type = void>
using Require = std::enable_if_t<Condition, Type>;

template <typename T>
using IsConnectiable = Require<Connectiable<T>>;
/**
* Connection is Connection Provider itself, so we need to define connection
* type it provides.
*/
template <typename T>
struct get_connection_type<T, IsConnectiable<T>> {
    using type = std::decay_t<T>;
};


template <typename T, typename = std::void_t<>>
struct has_operator_not : std::false_type {};
template <typename T>
struct has_operator_not<T, std::void_t<decltype(!std::declval<T>())>>
    : std::true_type {};

template <typename T>
constexpr auto OperatorNot = has_operator_not<std::decay_t<T>>::value;
/**
* Connection public traits section 
*/

template <typename T>
inline decltype(auto) unwrap_connection(T&& conn, 
        Require<Connection<T>>* = 0) noexcept {
    return std::forward<T>(conn);
}

template <typename T>
inline decltype(auto) unwrap_connection(T&& conn, 
        Require<ConnectionWrapper<T>>* = 0) noexcept {
    return *conn;
}

template <typename T, typename = IsConnectiable<T>>
inline decltype(auto) get_io_context(T& conn) noexcept {
    using impl::get_connection_io_context;
    return get_connection_io_context(unwrap_connection(conn));
}

template <typename T, typename = IsConnectiable<T>>
inline decltype(auto) get_handle(T&& conn) noexcept {
    using impl::get_connection_handle;
    return get_connection_handle(
        unwrap_connection(std::forward<T>(conn)));
}

template <typename T, typename = IsConnectiable<T>>
inline decltype(auto) get_native_handle(T&& conn) noexcept {
    return get_handle(std::forward<T>(conn)).get();
}

template <typename T, typename = IsConnectiable<T>>
inline decltype(auto) get_socket(T&& conn) noexcept {
    using impl::get_connection_socket;
    return get_connection_socket(unwrap_connection(std::forward<T>(conn)));
}

template <typename T>
inline Require<Connection<T>,
bool> connection_bad(const T& conn) noexcept { 
    using impl::connection_bad;
    return connection_bad(get_native_handle(conn));
}

template <typename T>
inline Require<ConnectionWrapper<T> && OperatorNot<T>,
bool> connection_bad(const T& conn) noexcept {
    using impl::connection_bad;
    return !conn || connection_bad(get_native_handle(conn));
}

template <typename T, typename = IsConnectiable<T>>
inline bool connection_good(const T& conn) noexcept { 
    return !connection_bad(conn);
}

template <typename T, typename = IsConnectiable<T>>
inline decltype(auto) get_oid_map(T&& conn) noexcept {
    using impl::get_connection_oid_map;
    return get_connection_oid_map(unwrap_connection(std::forward<T>(conn)));
}

template <typename T, typename = IsConnectiable<T>>
inline decltype(auto) get_statistics(T&& conn) noexcept {
    using impl::get_connection_statistics;
    return get_connection_statistics(unwrap_connection(std::forward<T>(conn)));
}

// Oid Map helpers

template <typename T, typename C, typename = IsConnectiable<C>>
inline decltype(auto) type_oid(C&& conn) noexcept {
    return type_oid<std::decay_t<T>>(
        get_oid_map(std::forward<C>(conn)));
}

template <typename T, typename C, typename = IsConnectiable<C>>
inline decltype(auto) type_oid(C&& conn, const T&) noexcept {
    return type_oid<std::decay_t<T>>(std::forward<C>(conn));
}

template <typename T, typename C, typename = IsConnectiable<C>>
inline void set_type_oid(C&& conn, oid_t oid) noexcept {
    set_type_oid<T>(get_oid_map(std::forward<C>(conn)), oid);
}

/**
* Function to get connection from provider asynchronously via callback. 
* This is customization point which allows to work with different kinds
* of connection providing. E.g. single connection, get connection from 
* pool, lazy connection, retriable connection and so on. 
*/
template <typename T, typename Handler>
inline Require<ConnectionProviderFunctor<T>>
async_get_connection(T&& provider, Handler&& handler) {
    provider(std::forward<Handler>(handler));
}

template <typename T, typename Handler>
inline Require<Connection<T> || ConnectionWrapper<T>>
async_get_connection(T&& conn, Handler&& handler) {
    decltype(auto) io = get_connection_io_context(conn);
    io.dispatch(detail::bind(
        std::forward<Handler>(handler), error_code{}, std::forward<T>(conn)));
}

template <typename, typename = std::void_t<>>
struct is_connection_provider : std::false_type {};
template <typename T>
struct is_connection_provider<T, std::void_t<
    decltype( async_get_connection(std::declval<T&>(), std::declval<void(error_code, connection_type<T>)>()) )
>> : std::true_type {};

template <typename T>
constexpr auto ConnectionProvider = is_connection_provider<std::decay_t<T>>::value;

/**
* Function to get a connection from provider. 
* Accepts:
*   provider - connection provider which will be asked for connection
*   token - completion token which determine the continuation of operation
*           it can be callback, yield_context, use_future and other Boost.Asio
*           compatible tokens.
* Returns:
*   completion token depent value like void for callback, connection for the
*   yield_context, std::future<connection> for use_future, and so on.
*/
template <typename T, typename CompletionToken, typename = Require<ConnectionProvider<T>>>
auto get_connection(T&& provider, CompletionToken&& token) {
    using signature_t = void (error_code, connection_type<T>);
    async_completion<CompletionToken, signature_t> init(std::forward<CompletionToken>(token));

    async_get_connection(std::forward<T>(provider), 
            std::move(init.handler));

    return init.result.get();
}

} // namespace libapq
