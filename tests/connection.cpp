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
    friend bool connection_bad(const test_conn_handle* h) { 
        return h->v == native_handle::bad;
    }
};

using empty_oid_map = decltype(libapq::register_types<>());

template <typename OidMap = empty_oid_map>
struct connection_ctx {
    std::unique_ptr<test_conn_handle> handle_;
    int socket_ = 0;
    OidMap oid_map_;

    friend OidMap& get_connection_oid_map(connection_ctx& conn) {
        return conn.oid_map_;
    }
    friend const OidMap& get_connection_oid_map(const connection_ctx& conn) {
        return conn.oid_map_;
    }
    friend int& get_connection_socket(connection_ctx& conn) {
        return conn.socket_;
    }
    friend const int& get_connection_socket(const connection_ctx& conn) {
        return conn.socket_;
    }
    friend std::unique_ptr<test_conn_handle>& 
    get_connection_handle(connection_ctx& conn) {
        return conn.handle_;
    }
    friend const std::unique_ptr<test_conn_handle>& 
    get_connection_handle(const connection_ctx& conn) {
        return conn.handle_;
    }
};

template <typename ...Ts>
using connection = std::shared_ptr<connection_ctx<Ts...>>;

GTEST("libapq::connection_good()", "[for object with bad handle returns false]") {
    auto ctx = std::make_shared<connection_ctx<>>();
    ctx->handle_ = std::make_unique<test_conn_handle>(native_handle::bad);
    connection<> conn{std::move(ctx)};
    EXPECT_FALSE(libapq::connection_good(conn));
}

GTEST("libapq::connection_good()", "[for object with nullptr returns false]") {
    connection<> conn;
    EXPECT_FALSE(libapq::connection_good(conn));
}

GTEST("libapq::connection_good()", "[for object with good handle returns true]") {
    auto ctx = std::make_shared<connection_ctx<>>();
    ctx->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
    connection<> conn{std::move(ctx)};
    EXPECT_TRUE(libapq::connection_good(conn));
}

GTEST("libapq::connection_bad()", "[for object with bad handle returns true]") {
    auto ctx = std::make_shared<connection_ctx<>>();
    ctx->handle_ = std::make_unique<test_conn_handle>(native_handle::bad);
    connection<> conn{std::move(ctx)};
    EXPECT_TRUE(libapq::connection_bad(conn));
}

GTEST("libapq::connection_bad()", "[for object with nullptr returns true]") {
    connection<> conn;
    EXPECT_FALSE(libapq::connection_good(conn));
}

GTEST("libapq::connection_bad()", "[for object with good handle returns false]") {
    auto ctx = std::make_shared<connection_ctx<>>();
    ctx->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
    connection<> conn{std::move(ctx)};
    EXPECT_FALSE(libapq::connection_bad(conn));
}

GTEST("ZZZZ", "[ZZZZZ]") {
    EXPECT_FALSE(libapq::is_connection<int>::value);
}
