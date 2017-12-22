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
    friend bool connection_status_bad(const test_conn_handle* h) { 
        return h->v == native_handle::bad;
    }
};

using empty_oid_map = decltype(libapq::register_types<>());

template <typename OidMap = empty_oid_map>
struct connection {
    using handle_type = std::unique_ptr<test_conn_handle>;

    handle_type handle_;
    int socket_ = 0;
    OidMap oid_map_;

    friend OidMap& get_connection_oid_map(connection& conn) {
        return conn.oid_map_;
    }
    friend const OidMap& get_connection_oid_map(const connection& conn) {
        return conn.oid_map_;
    }
    friend int& get_connection_socket(connection& conn) {
        return conn.socket_;
    }
    friend const int& get_connection_socket(const connection& conn) {
        return conn.socket_;
    }
    friend handle_type& get_connection_handle(connection& conn) {
        return conn.handle_;
    }
    friend const handle_type& get_connection_handle(const connection& conn) {
        return conn.handle_;
    }
};

template <typename ...Ts>
using connection_ptr = std::shared_ptr<connection<Ts...>>;

static_assert(libapq::Connection<connection<>>,
    "connection does not meet Connection requirements");
static_assert(libapq::ConnectionWrapper<connection_ptr<>>,
    "connection_ptr does not meet ConnectionWrapper requirements");
static_assert(libapq::Connectiable<connection<>>,
    "connection does not meet Connectiable requirements");
static_assert(libapq::Connectiable<connection_ptr<>>,
    "connection_ptr does not meet Connectiable requirements");

static_assert(!libapq::Connection<int>,
    "int meets Connection requirements unexpectedly");
static_assert(!libapq::ConnectionWrapper<int>,
    "int meets ConnectionWrapper requirements unexpectedly");
static_assert(!libapq::Connectiable<int>,
    "int meets Connectiable requirements unexpectedly");

GTEST("libapq::connection_good()") {
    SHOULD("for object with bad handle returns false") {
        auto conn = std::make_shared<connection<>>();
        conn->handle_ = std::make_unique<test_conn_handle>(native_handle::bad);
        EXPECT_FALSE(libapq::connection_good(conn));
    }

    SHOULD("for object with nullptr returns false") {
        connection_ptr<> conn;
        EXPECT_FALSE(libapq::connection_good(conn));
    }

    SHOULD("for object with good handle returns true") {
        auto conn = std::make_shared<connection<>>();
        conn->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
        EXPECT_TRUE(libapq::connection_good(conn));
    }
}

GTEST("libapq::connection_bad()") {
    SHOULD("for object with bad handle returns true") {
        auto conn = std::make_shared<connection<>>();
        conn->handle_ = std::make_unique<test_conn_handle>(native_handle::bad);
        EXPECT_TRUE(libapq::connection_bad(conn));
    }

    SHOULD("for object with nullptr returns true") {
        connection_ptr<> conn;
        EXPECT_FALSE(libapq::connection_good(conn));
    }

    SHOULD("for object with good handle returns false") {
        auto conn = std::make_shared<connection<>>();
        conn->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
        EXPECT_FALSE(libapq::connection_bad(conn));
    }
}
