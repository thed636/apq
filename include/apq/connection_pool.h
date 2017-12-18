#pragma once

#include <apq/connection.h>
#include <yamail/resource_pool/async/pool.hpp>

namespace libapq {

namespace pool = yamail::resource_pool::async;

template <typename OidMap>
using connection_pool_impl = pool::pool<connection_context<OidMap>>;

template <typename OidMap>
class pooled_connection_context {
    using handle_type = typename connection_pool_impl<OidMap>::handle;

public:
    pooled_connection_context(handle_type&& handle)
    : handle_(std::move(handle)) {}

    decltype(auto) oid_map() noexcept {return handle_->oid_map();}
    decltype(auto) handle() const noexcept {return handle_->handle();}
    decltype(auto) socket() noexcept {return handle_->socket();}
    decltype(auto) statistics() noexcept {return handle_->statistics();}

private:
    handle_type handle_;
};

template <typename OidMap>
using pooled_connection = connection<pooled_connection_context<OidMap>>;

class connection_pool {
public:
    connection_pool() {}
};

} // namespace apq
