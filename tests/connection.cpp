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
using connection = libapq::connection<std::shared_ptr<connection_ctx<OidMap>>>;

// namespace libapq {
// namespace detail {

template <typename OidMap>
inline decltype(auto) get_connection_context(const std::shared_ptr<connection_ctx<OidMap>>& ptr) {
    return *ptr;
}

// }
// }


template <typename OidMap>
inline bool connection_bad(const connection_ctx<OidMap>& ctx) { 
    return ctx.handle_->v == native_handle::bad;
}

GTEST("libapq::connection::operator bool()", "[default constructed object returns false]") {
    connection<> conn;
    EXPECT_FALSE(conn);
}

GTEST("libapq::connection::operator bool()", "[object with bad handle returns false]") {
    auto ctx = std::make_shared<connection_ctx<>>();
    ctx->handle_ = std::make_unique<test_conn_handle>(native_handle::bad);
    connection<> conn{std::move(ctx)};
    EXPECT_FALSE(conn);
}


GTEST("libapq::connection::operator bool()", "[object with good handle returns true]") {
    auto ctx = std::make_shared<connection_ctx<>>();
    ctx->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
    connection<> conn{std::move(ctx)};
    EXPECT_TRUE(conn);
}

GTEST("libapq::connection::operator !()", "[default constructed object returns true]") {
    connection<> conn;
    EXPECT_TRUE(!conn);
}

GTEST("libapq::connection::operator !()", "[object with bad handle returns true]") {
    auto ctx = std::make_shared<connection_ctx<>>();
    ctx->handle_ = std::make_unique<test_conn_handle>(native_handle::bad);
    connection<> conn{std::move(ctx)};
    EXPECT_TRUE(!conn);
}

GTEST("libapq::connection::operator !()", "[object with good handle returns false]") {
    auto ctx = std::make_shared<connection_ctx<>>();
    ctx->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
    connection<> conn{std::move(ctx)};
    EXPECT_FALSE(!conn);
}

GTEST("libapq::connection::operator ==()", "[for objects with same handle returns true]") {
    auto ctx = std::make_shared<connection_ctx<>>();
    ctx->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
    connection<> conn1{ctx};
    connection<> conn2{ctx};
    EXPECT_TRUE(conn1 == conn2);
}

GTEST("libapq::connection::operator ==()", "[for objects with different handle returns false]") {
    auto ctx1 = std::make_shared<connection_ctx<>>();
    ctx1->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
    connection<> conn1{ctx1};
    auto ctx2 = std::make_shared<connection_ctx<>>();
    ctx2->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
    connection<> conn2{ctx2};
    EXPECT_FALSE(conn1 == conn2);
}

GTEST("libapq::connection::operator !=()", "[for objects with same handle returns false]") {
    auto ctx = std::make_shared<connection_ctx<>>();
    ctx->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
    connection<> conn1{ctx};
    connection<> conn2{ctx};
    EXPECT_FALSE(conn1 != conn2);
}

GTEST("libapq::connection::operator !=()", "[object with different handle returns true]") {
    auto ctx1 = std::make_shared<connection_ctx<>>();
    ctx1->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
    connection<> conn1{ctx1};
    auto ctx2 = std::make_shared<connection_ctx<>>();
    ctx2->handle_ = std::make_unique<test_conn_handle>(native_handle::good);
    connection<> conn2{ctx2};
    EXPECT_TRUE(conn1 != conn2);
}
