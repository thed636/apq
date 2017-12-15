#include <GUnit/GTest.h>

#include <apq/query_builder.h>

GTEST("libapq::detail::to_string", "[with 0 returns \"0\"_s]") {
    using namespace boost::hana::literals;
    EXPECT_EQ(libapq::detail::to_string<std::size_t(0)>(), "0"_s);
}

GTEST("libapq::detail::to_string", "[with one digit number returns string with same digit]") {
    using namespace boost::hana::literals;
    EXPECT_EQ(libapq::detail::to_string<std::size_t(7)>(), "7"_s);
}

GTEST("libapq::detail::to_string", "[with two digits number returns string with digits in same order]") {
    using namespace boost::hana::literals;
    EXPECT_EQ(libapq::detail::to_string<std::size_t(42)>(), "42"_s);
}

GTEST("libapq::query_builder::text", "[with one text element returns input]") {
    using namespace libapq::literals;
    using namespace boost::hana::literals;
    EXPECT_EQ("SELECT 1"_SQL.text(), "SELECT 1"_s);
}

GTEST("libapq::query_builder::text", "[with two text elements returns concatenation]") {
    using namespace libapq::literals;
    using namespace boost::hana::literals;
    EXPECT_EQ(("SELECT 1"_SQL + " + 1"_SQL).text(), "SELECT 1 + 1"_s);
}

GTEST("libapq::query_builder::text", "[with text and int32 param elements returns text with placeholder for param]") {
    using namespace libapq::literals;
    using namespace boost::hana::literals;
    EXPECT_EQ(("SELECT "_SQL + std::int32_t(42)).text(), "SELECT $1"_s);
}

GTEST("libapq::query_builder::text", "[with text and two int32 params elements returns text with placeholders for each param]") {
    using namespace libapq::literals;
    using namespace boost::hana::literals;
    EXPECT_EQ(("SELECT "_SQL + std::int32_t(42) + " + "_SQL + std::int32_t(42)).text(), "SELECT $1 + $2"_s);
}

GTEST("libapq::query_builder::params", "[with one text element returns empty tuple]") {
    using namespace libapq::literals;
    EXPECT_EQ("SELECT 1"_SQL.params(), boost::hana::tuple<>());
}

GTEST("libapq::query_builder::params", "[with text and int32 param elements returns tuple with one value]") {
    using namespace libapq::literals;
    EXPECT_EQ(("SELECT "_SQL + std::int32_t(42)).params(), boost::hana::make_tuple(std::int32_t(42)));
}

GTEST("libapq::query_builder::params", "[with text and not null pointer param elements returns tuple with one value]") {
    using namespace libapq::literals;
    const auto ptr = std::make_unique<std::int32_t>(42);
    const auto params = ("SELECT "_SQL + ptr.get()).params();
    EXPECT_EQ(*params[boost::hana::size_c<0>], std::int32_t(42));
}

GTEST("libapq::query_builder::build", "[with one text element returns query with text equal to input]") {
    using namespace libapq::literals;
    EXPECT_STREQ("SELECT 1"_SQL.build().text(), "SELECT 1");
}

GTEST("libapq::query_builder::build", "[with one text element returns query with 0 param]") {
    using namespace libapq::literals;
    EXPECT_EQ("SELECT 1"_SQL.build().params_count, 0);
}

GTEST("libapq::query_builder::build", "[with text and int32 param elements returns query with 1 param]") {
    using namespace libapq::literals;
    EXPECT_EQ(("SELECT "_SQL + std::int32_t(42)).build().params_count, 1);
}

GTEST("libapq::query_builder::build", "[with text and reference_wrapper param elements returns query with 1 param]") {
    using namespace libapq::literals;
    const auto value = 42.13f;
    const auto query = ("SELECT "_SQL + std::cref(value)).build();
    EXPECT_EQ(*reinterpret_cast<const float*>(query.values()[0]), 42.13f);
}

GTEST("libapq::query_builder::build", "[with text and not null std::unique_ptr param elements returns query with 1 param]") {
    using namespace libapq::literals;
    const auto ptr = std::make_unique<float>(42.13f);
    const auto query = ("SELECT "_SQL + ptr).build();
    EXPECT_EQ(*reinterpret_cast<const float*>(query.values()[0]), 42.13f);
}

GTEST("libapq::query_builder::build", "[with text and not null std::shared_ptr param elements returns query with 1 param]") {
    using namespace libapq::literals;
    const auto ptr = std::make_shared<float>(42.13f);
    const auto query = ("SELECT "_SQL + ptr).build();
    EXPECT_EQ(*reinterpret_cast<const float*>(query.values()[0]), 42.13f);
}
