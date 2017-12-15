#include <GUnit/GTest.h>

#include <apq/query.h>

#include <iterator>

GTEST("libapq::query.params_count", "[without parameters should be equal to 0]") {
    const auto query = libapq::make_query("");
    EXPECT_EQ(query.params_count, 0);
}

GTEST("libapq::query", "[with more than 0 parameters should be equal to that number]") {
    const auto query = libapq::make_query("", true, 42, std::string("text"));
    EXPECT_EQ(query.params_count, 3);
}

GTEST("libapq::query::text", "[query text should be equal to input]") {
    const auto query = libapq::make_query("query");
    EXPECT_STREQ(query.text(), "query");
}

GTEST("libapq::query::types", "[type of the param should be equal to type oid]") {
    const auto query = libapq::make_query("", std::int16_t());
    EXPECT_EQ(query.types()[0], libapq::type_traits<std::int16_t>::oid());
}

GTEST("libapq::query::types", "[nullptr type should be equal to 0]") {
    const auto query = libapq::make_query("", nullptr);
    EXPECT_EQ(query.types()[0], 0);
}

GTEST("libapq::query::types", "[null std::optional<std::int32_t> type should be equal to std::int32_t oid]") {
    const auto query = libapq::make_query("", std::optional<std::int32_t>());
    EXPECT_EQ(query.types()[0], libapq::type_traits<std::int32_t>::oid());
}

GTEST("libapq::query::types", "[null std::shared_ptr<std::int32_t> type should be equal to std::int32_t oid]") {
    const auto query = libapq::make_query("", std::shared_ptr<std::int32_t>());
    EXPECT_EQ(query.types()[0], libapq::type_traits<std::int32_t>::oid());
}

GTEST("libapq::query::types", "[null std::unique_ptr<std::int32_t> type should be equal to std::int32_t oid]") {
    const auto query = libapq::make_query("", std::vector<std::int32_t>());
    EXPECT_EQ(query.types()[0], libapq::type_traits<std::vector<std::int32_t>>::oid());
}

GTEST("libapq::query::types", "[null std::weak_ptr<std::int32_t> type should be equal to std::int32_t oid]") {
    const auto query = libapq::make_query("", std::weak_ptr<std::int32_t>());
    EXPECT_EQ(query.types()[0], libapq::type_traits<std::int32_t>::oid());
}

GTEST("libapq::query::types", "[std::vector<float> type should be equal to std::vector<float> oid]") {
    const auto value = 42.13f;
    const auto query = libapq::make_query("", std::cref(value));
    EXPECT_EQ(*reinterpret_cast<const float*>(query.values()[0]), 42.13f);
}

GTEST("libapq::query::formats", "[format of the param should be equal to 1]") {
    const auto query = libapq::make_query("", std::int16_t());
    EXPECT_EQ(query.formats()[0], 1);
}

GTEST("libapq::query::lengths", "[length of the param should be equal to its binary serialized data size]") {
    const auto query = libapq::make_query("", std::int16_t());
    EXPECT_EQ(query.lengths()[0], sizeof(std::int16_t));
}

GTEST("libapq::query::values", "[value of the floating point type param should be equal to input]") {
    const auto query = libapq::make_query("", 42.13f);
    EXPECT_EQ(*reinterpret_cast<const float*>(query.values()[0]), 42.13f);
}

GTEST("libapq::query::lengths", "[string length should be equal to number of chars]") {
    const auto query = libapq::make_query("", std::string("std::string"));
    EXPECT_EQ(query.lengths()[0], 11);
}

GTEST("libapq::query::values", "[string value should be equal to input]") {
    using namespace ::testing;
    const auto query = libapq::make_query("", std::string("string"));
    EXPECT_THAT(std::vector<char>(query.values()[0], query.values()[0] + 6),
        ElementsAre('s', 't', 'r', 'i', 'n', 'g'));
}

GTEST("libapq::query::values", "[nullptr value should be equal to nullptr]") {
    const auto query = libapq::make_query("", nullptr);
    EXPECT_EQ(query.values()[0], nullptr);
}

GTEST("libapq::query::values", "[nullopt value should be equal to nullptr]") {
    const auto query = libapq::make_query("", std::nullopt);
    EXPECT_EQ(query.values()[0], nullptr);
}

GTEST("libapq::query::values", "[null std::optional<std::int32_t> value should be equal to nullptr]") {
    const auto query = libapq::make_query("", std::optional<std::int32_t>());
    EXPECT_EQ(query.values()[0], nullptr);
}

GTEST("libapq::query::values", "[not null std::optional<std::string> value should be equal to input]") {
    using namespace ::testing;
    const auto query = libapq::make_query("", std::optional<std::string>("string"));
    EXPECT_THAT(std::vector<char>(query.values()[0], query.values()[0] + 6),
        ElementsAre('s', 't', 'r', 'i', 'n', 'g'));
}

GTEST("libapq::query::values", "[null std::shared_ptr<std::int32_t> value should be equal to nullptr]") {
    const auto query = libapq::make_query("", std::shared_ptr<std::int32_t>());
    EXPECT_EQ(query.values()[0], nullptr);
}

GTEST("libapq::query::values", "[not null std::shared_ptr<std::string> value should be equal to input]") {
    using namespace ::testing;
    const auto query = libapq::make_query("", std::make_shared<std::string>("string"));
    EXPECT_THAT(std::vector<char>(query.values()[0], query.values()[0] + 6),
        ElementsAre('s', 't', 'r', 'i', 'n', 'g'));
}

GTEST("libapq::query::values", "[null std::unique_ptr<std::int32_t> value should be equal to nullptr]") {
    const auto query = libapq::make_query("", std::unique_ptr<std::int32_t>());
    EXPECT_EQ(query.values()[0], nullptr);
}

GTEST("libapq::query::values", "[not null std::unique_ptr<std::string> value should be equal to input]") {
    using namespace ::testing;
    const auto query = libapq::make_query("", std::make_unique<std::string>("string"));
    EXPECT_THAT(std::vector<char>(query.values()[0], query.values()[0] + 6),
        ElementsAre('s', 't', 'r', 'i', 'n', 'g'));
}

GTEST("libapq::query::values", "[null std::weak_ptr<std::int32_t> value should be equal to nullptr]") {
    const auto query = libapq::make_query("", std::weak_ptr<std::int32_t>());
    EXPECT_EQ(query.values()[0], nullptr);
}

GTEST("libapq::query::values", "[not null std::weak_ptr<std::string> value should be equal to input]") {
    using namespace ::testing;
    const auto ptr = std::make_shared<std::string>("string");
    const auto query = libapq::make_query("", std::weak_ptr<std::string>(ptr));
    EXPECT_THAT(std::vector<char>(query.values()[0], query.values()[0] + 6),
        ElementsAre('s', 't', 'r', 'i', 'n', 'g'));
}

GTEST("libapq::query::values", "[std::reference_wrapper<std::int8_t> value should be equal to input]") {
    const auto value = 42.13f;
    const auto query = libapq::make_query("", std::cref(value));
    EXPECT_EQ(*reinterpret_cast<const float*>(query.values()[0]), 42.13f);
}
