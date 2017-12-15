#include <GUnit/GTest.h>

#include <apq/binary_serialization.h>

GTEST("libapq::send", "[with std::int8_t should return as is]") {
    using namespace testing;
    std::vector<char> buffer;
    libapq::send(std::int8_t(42), libapq::register_types<>(), std::back_inserter(buffer));
    EXPECT_THAT(buffer, ElementsAre(42));
}

GTEST("libapq::send", "[with std::int16_t should return bytes in big-endian order]") {
    using namespace testing;
    std::vector<char> buffer;
    libapq::send(std::int16_t(42), libapq::register_types<>(), std::back_inserter(buffer));
    EXPECT_THAT(buffer, ElementsAre(0, 42));
}

GTEST("libapq::send", "[with std::int32_t should return bytes in big-endian order]") {
    using namespace testing;
    std::vector<char> buffer;
    libapq::send(std::int32_t(42), libapq::register_types<>(), std::back_inserter(buffer));
    EXPECT_THAT(buffer, ElementsAre(0, 0, 0, 42));
}

GTEST("libapq::send", "[with std::int64_t should return bytes in big-endian order]") {
    using namespace testing;
    std::vector<char> buffer;
    libapq::send(std::int64_t(42), libapq::register_types<>(), std::back_inserter(buffer));
    EXPECT_THAT(buffer, ElementsAre(0, 0, 0, 0, 0, 0, 0, 42));
}

GTEST("libapq::send", "[with float should return bytes in same order]") {
    using namespace testing;
    std::vector<char> buffer;
    libapq::send(42.13f, libapq::register_types<>(), std::back_inserter(buffer));
    EXPECT_THAT(buffer, ElementsAre(0x1F, 0x85, 0x28, 0x42));
}

GTEST("libapq::send", "[with std::string should return bytes in same order]") {
    using namespace testing;
    std::vector<char> buffer;
    libapq::send(std::string("text"), libapq::register_types<>(), std::back_inserter(buffer));
    EXPECT_THAT(buffer, ElementsAre('t', 'e', 'x', 't'));
}

GTEST("libapq::send", "[with std::vector<float> should return bytes with one dimension array header]") {
    using namespace testing;
    std::vector<char> buffer;
    libapq::send(std::vector<float>(), libapq::register_types<>(), std::back_inserter(buffer));
    EXPECT_EQ(buffer, std::vector<char>({
        0, 0, 0, 1,
        0, 0, 0, 0,
        0, 0, 2, '\xBC',
        0, 0, 0, 0,
        0, 0, 0, 1
    }));
}
