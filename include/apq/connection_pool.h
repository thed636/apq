#pragma once

#include <apq/connection.h>
#include <yamail/resource_pool/async/pool.hpp>

namespace libapq {

namespace pool = yamail::resource_pool::async;



namespace impl {

template <typename OidMap, typename Statistics>
using connection_pool = pool::pool<connection_context<OidMap, Statistics>>;

template <typename OidMap, typename Statistics>
using pooled_connection_ctx_ptr = std::shared_ptr<typename connection_pool<OidMap, Statistics>::handle>;

}

namespace detail {

template <typename OidMap, typename Statistics>
inline decltype(auto) get_context(const impl::pooled_connection_ctx_ptr<OidMap, Statistics>& ptr) {
    return ptr->get();
}

}

class connection_pool {
public:
    connection_pool() {}
};

} // namespace apq
