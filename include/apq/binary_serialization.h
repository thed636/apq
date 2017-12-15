#pragma once

#include <apq/type_traits.h>

#include <optional>
#include <type_traits>

#include <endian.h>

namespace libapq {
namespace detail {

template <class OutIteratorT, class T, class = std::enable_if_t<std::is_arithmetic_v<T>>>
constexpr void write(T value, OutIteratorT out) {
    std::copy(
        reinterpret_cast<const char*>(&value),
        reinterpret_cast<const char*>(&value) + sizeof(value),
        out
    );
}

template <class T>
constexpr std::size_t get_dimension(const T&) noexcept {
    return 1;
}

template <class T>
constexpr std::enable_if_t<std::is_arithmetic_v<T>, std::size_t> get_size(const T&) noexcept {
    return sizeof(T);
}

template <class T>
constexpr std::enable_if_t<!std::is_arithmetic_v<T>, std::size_t> get_size(const T& value) noexcept {
    return std::size(value);
}

} // namespace detail

template <class T>
using integral_8_t = std::enable_if_t<std::is_integral_v<T> && sizeof(T) == sizeof(std::uint8_t), void>;

template <class T, class OidMapT, class OutIteratorT>
constexpr integral_8_t<T> send(T value, const OidMapT&, OutIteratorT out) {
    detail::write(value, out);
}

template <class T>
using integral_16_t = std::enable_if_t<std::is_integral_v<T> && sizeof(T) == sizeof(std::uint16_t), void>;

template <class T, class OidMapT, class OutIteratorT>
integral_16_t<T> send(T value, const OidMapT&, OutIteratorT out) {
    detail::write(htobe16(std::uint16_t(value)), out);
}

template <class T>
using integral_32_t = std::enable_if_t<std::is_integral_v<T> && sizeof(T) == sizeof(std::uint32_t), void>;

template <class T, class OidMapT, class OutIteratorT>
integral_32_t<T> send(T value, const OidMapT&, OutIteratorT out) {
    detail::write(htobe32(std::uint32_t(value)), out);
}

template <class T>
using integral_64_t = std::enable_if_t<std::is_integral_v<T> && sizeof(T) == sizeof(std::uint64_t), void>;

template <class T, class OidMapT, class OutIteratorT>
integral_64_t<T> send(T value, const OidMapT&, OutIteratorT out) {
    detail::write(htobe64(std::uint64_t(value)), out);
}

template <class T>
using floating_point_t = std::enable_if_t<std::is_floating_point_v<T> && (sizeof(T) == 4 || sizeof(T) == 8), void>;

template <class T, class OidMapT, class OutIteratorT>
constexpr floating_point_t<T> send(T value, const OidMapT&, OutIteratorT out) {
    detail::write(value, out);
}

template <class OidMapT, class OutIteratorT, class CharT, class TraitsT, class AllocatorT>
constexpr void send(const std::basic_string<CharT, TraitsT, AllocatorT>& value, const OidMapT&, OutIteratorT out) {
    std::for_each(value.begin(), value.end(), [&] (auto v) { detail::write(v, out); });
}

template <class OidMapT, class OutIteratorT, class T, class AllocatorT>
constexpr void send(const std::vector<T, AllocatorT>& value, const OidMapT& oid_map, OutIteratorT out) {
    send(std::int32_t(detail::get_dimension(value)), oid_map, out);
    send(std::int32_t(0), oid_map, out);
    send(type_oid<T>(oid_map), oid_map, out);
    send(std::int32_t(std::size(value)), oid_map, out);
    send(std::int32_t(1), oid_map, out);
    std::for_each(value.begin(), value.end(),
        [&] (const auto& v) {
            send(detail::get_size(v), oid_map, out);
            send(v, oid_map, out);
        });
}

template <class OidMapT, class OutIteratorT, class AllocatorT>
constexpr void send(const std::vector<char, AllocatorT>& value, const OidMapT&, OutIteratorT out) {
    std::for_each(value.begin(), value.end(), [&] (auto v) { detail::write(v, out); });
}

template <class OidMapT, class OutIteratorT>
constexpr void send(const boost::uuids::uuid& value, const OidMapT&, OutIteratorT out) {
    std::for_each(value.begin(), value.end(), [&] (auto v) { detail::write(v, out); });
}

} // namespace apq
