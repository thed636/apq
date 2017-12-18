#pragma once

#include <boost/system/system_error.hpp>

namespace libapq {

using error_code = ::boost::system::error_code;
using system_error = ::boost::system::system_error;

inline auto make_error_code(::boost::system::error_code code,
        const std::string& /*msg*/) {
    return code;
}

} // namespace libapq
