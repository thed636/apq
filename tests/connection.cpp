#include <apq/connection.h>

#include <boost/make_shared.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/fusion/adapted/struct/define_struct.hpp>
#include <boost/fusion/include/define_struct.hpp>

#include <boost/hana/adapt_adt.hpp>

#include <GUnit/GTest.h>

namespace hana = ::boost::hana;

enum class native_handle { bad, good };

struct test_conn_handle {
    native_handle v;
    test_conn_handle(native_handle v) : v(v) {}
    friend bool connection_bad(test_conn_handle* h) { 
        return h->v == native_handle::bad;
    }
};


using empty_oid_map = decltype(libapq::register_types<>());

template <typename OidMap = empty_oid_map>
struct connection_ctx {
    auto& oid_map() noexcept {return oid_map_;}
    auto& handle() const noexcept {return handle_;}
    auto& socket() noexcept {return socket_;}

    std::unique_ptr<test_conn_handle> handle_;
    int socket_ = 0;
    OidMap oid_map_;
};

template <typename OidMap = empty_oid_map>
using connection = libapq::basic_connection<connection_ctx<OidMap>>;

GTEST("libapq::connection::operator bool()", "[default constructed object returns false]") {
    connection<> conn;
    EXPECT_FALSE(conn);
}

GTEST("libapq::connection::operator bool()", "[object with bad handle returns false]") {
    auto impl = std::make_shared<connection_ctx<>>();
    impl->handle_ = std::make_unique<test_conn_handle>(native_handle::bad);
    connection<> conn{std::move(impl)};
    EXPECT_FALSE(conn);
}


GTEST("libapq::connection::operator bool()", "[object with good handle returns true]") {
    auto impl = std::make_shared<connection_ctx<>>();
    impl->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
    connection<> conn{std::move(impl)};
    EXPECT_TRUE(conn);
}

GTEST("libapq::connection::operator !()", "[default constructed object returns true]") {
    connection<> conn;
    EXPECT_TRUE(!conn);
}

GTEST("libapq::connection::operator !()", "[object with bad handle returns true]") {
    auto impl = std::make_shared<connection_ctx<>>();
    impl->handle_ = std::make_unique<test_conn_handle>(native_handle::bad);
    connection<> conn{std::move(impl)};
    EXPECT_TRUE(!conn);
}

GTEST("libapq::connection::operator !()", "[object with good handle returns false]") {
    auto impl = std::make_shared<connection_ctx<>>();
    impl->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
    connection<> conn{std::move(impl)};
    EXPECT_FALSE(!conn);
}
