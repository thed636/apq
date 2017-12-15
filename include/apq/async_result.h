#pragma once

#include <boost/asio/async_result.hpp>
#include <utility>
#include <apq/asio.h>

namespace libapq {

//We can reuse boost::asio::async_resul since it is not binded to error code type
using asio::async_result;
using asio::handler_type;

namespace detail {

template <typename Handler, typename Signature>
struct async_result_init {
    explicit async_result_init(Handler& orig_handler)
    : handler(std::move(orig_handler)),
      result(handler) {
    }

    using handler_type = typename libapq::handler_type<std::decay_t<Handler>, Signature>::type;
    handler_type handler;
    async_result<handler_type> result;
};

} //namespace detail
} // namespace libapq
