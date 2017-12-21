#pragma once

#include <apq/connection.h>
#include <yamail/resource_pool/async/pool.hpp>

namespace libapq {
namespace impl {

template <typename ...Ts>
using connection_pool = yamail::resource_pool::async::pool<connection<Ts...>>;

template <typename ...Ts>
using pooled_connection = yamail::resource_pool::handle<connection_pool<Ts...>>;

template <typename ...Ts>
using pooled_connection_ptr = std::shared_ptr<pooled_connection<Ts...>>;

} // namespace impl

template <typename T>
struct is_connection_wrapper<
    yamail::resource_pool::handle<
        yamail::resource_pool::async::pool<T>
    >> : is_connection<T> {};

template <typename T>
struct is_connection_wrapper< std::shared_ptr<
    yamail::resource_pool::handle<
        yamail::resource_pool::async::pool<T>
    >>> : is_connection<T> {};

static_assert(Connection<impl::pooled_connection_ptr<empty_oid_map, no_statistics>>, 
    "pooled_connection_ptr is not a Connection concept");

class connection_pool {
public:
    connection_pool() {}
};

} // namespace apq
