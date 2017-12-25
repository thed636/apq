#include <apq/impl/async_connect.h>
#include "test_error.h"
#include <GUnit/GTest.h>

namespace hana = ::boost::hana;

enum class native_handle {bad, good};

inline bool connection_status_bad(const native_handle* h) { 
    return *h == native_handle::bad;
}

inline std::string connection_error_message(const native_handle*) { 
    return "connection bad";
}

template <typename Handler>
void asio_post(Handler h) {
    using boost::asio::asio_handler_invoke;
    asio_handler_invoke(h, std::addressof(h));
}

struct socket_mock {
    socket_mock& get_io_context() { return *this;}
    template <typename Handler>
    void post(Handler&& h) {
        asio_post(std::forward<Handler>(h));
    }
};

using empty_oid_map = decltype(libapq::register_types<>());

struct connection_mock {
    using handler = std::function<void(libapq::error_code)>;

    virtual void write_poll(handler) const = 0;
    virtual void read_poll(handler) const = 0;
    virtual int connect_poll() const = 0;
    virtual libapq::error_code start_connection(const std::string&) = 0;
    virtual libapq::error_code assign_socket() = 0;
    virtual ~connection_mock() = default;
};

struct callback_mock {
    virtual void call(libapq::error_code) const = 0;
    virtual void context_preserved() const = 0;
    virtual ~callback_mock() = default;
};

struct callback_handler {
    callback_mock& mock;

    void operator() (libapq::error_code ec) const {
        mock.call(ec);
    }

    template <typename Func>
    friend void asio_handler_invoke(Func&& f, callback_handler* ctx) {
        ctx->mock.context_preserved();
        f();
    }
};

inline auto wrap(callback_mock& mock) {
    return callback_handler{mock};
}

template <typename OidMap = empty_oid_map>
struct connection {
    using handle_type = std::unique_ptr<native_handle>;

    handle_type handle_;
    socket_mock socket_;
    OidMap oid_map_;
    connection_mock* mock_ = nullptr;

    friend OidMap& get_connection_oid_map(connection& conn) {
        return conn.oid_map_;
    }
    friend const OidMap& get_connection_oid_map(const connection& conn) {
        return conn.oid_map_;
    }
    friend auto& get_connection_socket(connection& conn) {
        return conn.socket_;
    }
    friend const auto& get_connection_socket(const connection& conn) {
        return conn.socket_;
    }
    friend handle_type& get_connection_handle(connection& conn) {
        return conn.handle_;
    }
    friend const handle_type& get_connection_handle(const connection& conn) {
        return conn.handle_;
    }

    template <typename Handler>
    friend void write_poll(connection& c, Handler&& h) {
        c.mock_->write_poll([h] (auto e) {
            asio_post(libapq::detail::bind(std::move(h), std::move(e)));
        });
    }

    template <typename Handler>
    friend void read_poll(connection& c, Handler&& h) {
        c.mock_->read_poll([h] (auto e) {
            asio_post(libapq::detail::bind(std::move(h), std::move(e)));
        });
    }

    friend int connect_poll(connection& c) {
        return c.mock_->connect_poll();
    }

    friend libapq::error_code start_connection(
            connection& c, const std::string& conninfo) {
        return c.mock_->start_connection(conninfo);
    }

    friend libapq::error_code assign_socket(connection& c) {
        return c.mock_->assign_socket();
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

inline auto make_connection(connection_mock& mock) {
    return std::make_shared<connection<>>(connection<>{
            std::make_unique<native_handle>(native_handle::bad),
            socket_mock{},
            empty_oid_map{},
            std::addressof(mock)
        });
}

GTEST("libapq::async_connect()") {
    using namespace testing;
    using libapq::error_code;

    SHOULD("start connection, assign socket and wait for write complete") {
        StrictGMock<connection_mock> conn_mock{};
        StrictGMock<callback_mock> cb_mock{};
        auto conn = make_connection(object(conn_mock));
        *(conn->handle_) = native_handle::good;

        EXPECT_CALL(conn_mock, (start_connection)("conninfo")).WillOnce(Return(error_code{}));
        EXPECT_CALL(conn_mock, (assign_socket)()).WillOnce(Return(error_code{}));
        EXPECT_CALL(conn_mock, (write_poll)(_));
        libapq::impl::async_connect("conninfo", conn, wrap(object(cb_mock)));
    }

    SHOULD("call handler with pq_connection_start_failed on error in start_connection") {
        StrictGMock<connection_mock> conn_mock{};
        StrictGMock<callback_mock> cb_mock{};
        auto conn = make_connection(object(conn_mock));
        *(conn->handle_) = native_handle::good;

        EXPECT_CALL(conn_mock, (start_connection)("conninfo"))
            .WillOnce(Return(error_code{libapq::error::pq_connection_start_failed}));

        EXPECT_INVOKE(cb_mock, context_preserved);
        EXPECT_INVOKE(cb_mock, call, error_code{libapq::error::pq_connection_start_failed});

        libapq::impl::async_connect("conninfo", conn, wrap(object(cb_mock)));
    }

    SHOULD("call handler with pq_connection_status_bad if connection status is bad") {
        StrictGMock<connection_mock> conn_mock{};
        StrictGMock<callback_mock> cb_mock{};
        auto conn = make_connection(object(conn_mock));
        *(conn->handle_) = native_handle::bad;

        EXPECT_CALL(conn_mock, (start_connection)("conninfo")).WillOnce(Return(error_code{}));

        EXPECT_INVOKE(cb_mock, context_preserved);
        EXPECT_INVOKE(cb_mock, call, error_code{libapq::error::pq_connection_status_bad});

        libapq::impl::async_connect("conninfo", conn, wrap(object(cb_mock)));
    }

    SHOULD("call handler with error if assign_socket returns error") {
        StrictGMock<connection_mock> conn_mock{};
        StrictGMock<callback_mock> cb_mock{};
        auto conn = make_connection(object(conn_mock));
        *(conn->handle_) = native_handle::good;

        EXPECT_CALL(conn_mock, (start_connection)("conninfo")).WillOnce(Return(error_code{}));
        EXPECT_CALL(conn_mock, (assign_socket)()).WillOnce(Return(error_code{libapq::testing::error::error}));

        EXPECT_INVOKE(cb_mock, context_preserved);
        EXPECT_INVOKE(cb_mock, call, error_code{libapq::testing::error::error});

        libapq::impl::async_connect("conninfo", conn, wrap(object(cb_mock)));
    }

    SHOULD("wait for write complete if connect_poll() returns PGRES_POLLING_WRITING") {
        StrictGMock<connection_mock> conn_mock{};
        StrictGMock<callback_mock> cb_mock{};
        auto conn = make_connection(object(conn_mock));
        *(conn->handle_) = native_handle::good;

        testing::InSequence s;
        EXPECT_CALL(conn_mock, (start_connection)("conninfo")).WillOnce(Return(error_code{}));
        EXPECT_CALL(conn_mock, (assign_socket)()).WillOnce(Return(error_code{}));

        EXPECT_CALL(conn_mock, (write_poll)(_)).WillOnce(InvokeArgument<0>(error_code{}));
        EXPECT_INVOKE(cb_mock, context_preserved);

        EXPECT_CALL(conn_mock, (connect_poll)()).WillOnce(Return(PGRES_POLLING_WRITING));

        EXPECT_CALL(conn_mock, (write_poll)(_));
        libapq::impl::async_connect("conninfo", conn, wrap(object(cb_mock)));
    }

    SHOULD("wait for read complete if connect_poll() returns PGRES_POLLING_READING") {
        StrictGMock<connection_mock> conn_mock{};
        StrictGMock<callback_mock> cb_mock{};
        auto conn = make_connection(object(conn_mock));
        *(conn->handle_) = native_handle::good;

        testing::InSequence s;
        EXPECT_CALL(conn_mock, (start_connection)("conninfo")).WillOnce(Return(error_code{}));
        EXPECT_CALL(conn_mock, (assign_socket)()).WillOnce(Return(error_code{}));

        EXPECT_CALL(conn_mock, (write_poll)(_)).WillOnce(InvokeArgument<0>(error_code{}));
        EXPECT_INVOKE(cb_mock, context_preserved);

        EXPECT_CALL(conn_mock, (connect_poll)()).WillOnce(Return(PGRES_POLLING_READING));

        EXPECT_CALL(conn_mock, (read_poll)(_));
        libapq::impl::async_connect("conninfo", conn, wrap(object(cb_mock)));
    }

    SHOULD("call handler with no error if connect_poll() returns PGRES_POLLING_OK") {
        StrictGMock<connection_mock> conn_mock{};
        StrictGMock<callback_mock> cb_mock{};
        auto conn = make_connection(object(conn_mock));
        *(conn->handle_) = native_handle::good;

        testing::InSequence s;
        EXPECT_CALL(conn_mock, (start_connection)("conninfo")).WillOnce(Return(error_code{}));
        EXPECT_CALL(conn_mock, (assign_socket)()).WillOnce(Return(error_code{}));

        EXPECT_CALL(conn_mock, (write_poll)(_)).WillOnce(InvokeArgument<0>(error_code{}));
        EXPECT_INVOKE(cb_mock, context_preserved);

        EXPECT_CALL(conn_mock, (connect_poll)()).WillOnce(Return(PGRES_POLLING_OK));

        EXPECT_INVOKE(cb_mock, context_preserved);
        EXPECT_INVOKE(cb_mock, call, error_code{});
        libapq::impl::async_connect("conninfo", conn, wrap(object(cb_mock)));
    }

    SHOULD("call handler with pq_connect_poll_failed if connect_poll() returns the other code") {
        StrictGMock<connection_mock> conn_mock{};
        StrictGMock<callback_mock> cb_mock{};
        auto conn = make_connection(object(conn_mock));
        *(conn->handle_) = native_handle::good;

        testing::InSequence s;
        EXPECT_CALL(conn_mock, (start_connection)("conninfo")).WillOnce(Return(error_code{}));
        EXPECT_CALL(conn_mock, (assign_socket)()).WillOnce(Return(error_code{}));

        EXPECT_CALL(conn_mock, (write_poll)(_)).WillOnce(InvokeArgument<0>(error_code{}));
        EXPECT_INVOKE(cb_mock, context_preserved);

        EXPECT_CALL(conn_mock, (connect_poll)()).WillOnce(Return(-1));

        EXPECT_INVOKE(cb_mock, context_preserved);
        EXPECT_INVOKE(cb_mock, call, error_code{libapq::error::pq_connect_poll_failed});
        libapq::impl::async_connect("conninfo", conn, wrap(object(cb_mock)));
    }

    SHOULD("call handler with the error if polling operation invokes callback with it") {
        StrictGMock<connection_mock> conn_mock{};
        StrictGMock<callback_mock> cb_mock{};
        auto conn = make_connection(object(conn_mock));
        *(conn->handle_) = native_handle::good;

        testing::InSequence s;
        EXPECT_CALL(conn_mock, (start_connection)("conninfo")).WillOnce(Return(error_code{}));
        EXPECT_CALL(conn_mock, (assign_socket)()).WillOnce(Return(error_code{}));

        EXPECT_CALL(conn_mock, (write_poll)(_))
            .WillOnce(InvokeArgument<0>(libapq::testing::error::error));
        EXPECT_INVOKE(cb_mock, context_preserved);

        EXPECT_INVOKE(cb_mock, context_preserved);
        EXPECT_INVOKE(cb_mock, call, error_code{libapq::testing::error::error});
        libapq::impl::async_connect("conninfo", conn, wrap(object(cb_mock)));
    }
}
