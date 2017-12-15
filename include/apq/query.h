#include <apq/binary_serialization.h>
#include <apq/type_traits.h>

#include <boost/hana/for_each.hpp>
#include <boost/hana/tuple.hpp>

#include <libpq-fe.h>

#include <array>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

namespace libapq {

template <class T>
using not_nullopt_t = std::enable_if_t<!std::is_same_v<T, std::nullopt_t>, T>;

template <class T>
using is_nullable_t = std::enable_if_t<is_nullable<std::decay_t<T>>::value, void>;

template <class T>
using is_not_nullable_t = std::enable_if_t<!is_nullable<std::decay_t<T>>::value, void>;

template <class BufferAllocatorT, class OidMapT, class ... ArgsT>
class basic_query {
public:
    static constexpr std::size_t params_count = sizeof ... (ArgsT);

    using buffer_allocator_type = BufferAllocatorT;
    using oid_map_type = OidMapT;

    basic_query(const char* text)
        : impl(make_impl(buffer_allocator_type(), text, oid_map_type())) {}

    basic_query(const char* text, const not_nullopt_t<oid_map_type>& oid_map, const ArgsT& ... args)
        : impl(make_impl(buffer_allocator_type(), text, oid_map, args ...)) {}

    basic_query(const buffer_allocator_type& buffer_allocator, const char* text, const not_nullopt_t<oid_map_type>& oid_map, const ArgsT& ... args)
        : impl(make_impl(buffer_allocator, text, oid_map, args ...)) {}

    constexpr const char *text() const noexcept {
        return impl->text;
    }

    constexpr const ::Oid* types() const noexcept {
        return std::data(impl->types);
    }

    constexpr const int* formats() const noexcept {
        return std::data(impl->formats);
    }

    constexpr const int* lengths() const noexcept {
        return std::data(impl->lengths);
    }

    constexpr const char* const* values() const noexcept {
        return std::data(impl->values);
    }

private:
    using buffer_type = std::vector<char, buffer_allocator_type>;

    struct impl_type {
        const char* text;
        buffer_type buffer;
        std::array<::Oid, params_count> types;
        std::array<int, params_count> formats;
        std::array<int, params_count> lengths;
        std::array<const char*, params_count> values;

        impl_type(const char* text, const buffer_allocator_type& buffer_allocator)
            : text(text), buffer(buffer_allocator) {}

        impl_type(const impl_type& other) = delete;
        impl_type(impl_type&& other) = delete;
    };

    std::shared_ptr<impl_type> impl;

    static constexpr auto make_impl(const buffer_allocator_type& buffer_allocator, const char* text, const oid_map_type& oid_map, const ArgsT& ... args) {
        namespace hana = ::boost::hana;
        const auto tuple = hana::tuple<const ArgsT& ...>(args ...);
        auto result = std::make_shared<impl_type>(text, buffer_allocator);
        hana::for_each(
            hana::to<hana::tuple_tag>(hana::make_range(hana::size_c<0>, hana::size_c<params_count>)),
            [&] (auto field) { write_meta(oid_map, field, tuple[field], *result); }
        );
        std::size_t offset = 0;
        hana::for_each(
            hana::to<hana::tuple_tag>(hana::make_range(hana::size_c<0>, hana::size_c<params_count>)),
            [&] (auto field) {
                if (const auto size = result->lengths[field]) {
                    result->values[field] = std::data(result->buffer) + offset;
                    offset += size;
                } else {
                    result->values[field] = nullptr;
                }
            }
        );
        return result;
    }

    template <class T>
    static constexpr is_not_nullable_t<T> write_meta(const oid_map_type& oid_map, std::size_t field, const T& value, impl_type& result) {
        using libapq::send;
        result.types[field] = type_oid(oid_map, value);
        result.formats[field] = 1;
        const auto current_size = std::size(result.buffer);
        send(value, oid_map, std::back_inserter(result.buffer));
        result.lengths[field] = std::size(result.buffer) - current_size;
    }

    template <class T>
    static constexpr void write_typed_null_meta(const oid_map_type& oid_map, std::size_t field, impl_type& result) noexcept {
        result.types[field] = type_oid<T>(oid_map);
        result.formats[field] = 1;
        result.lengths[field] = 0;
    }

    static constexpr void write_null_meta(const oid_map_type&, std::size_t field, impl_type& result) noexcept {
        result.types[field] = 0;
        result.formats[field] = 1;
        result.lengths[field] = 0;
    }

    static constexpr void write_meta(const oid_map_type& oid_map, std::size_t field, std::nullptr_t, impl_type& result) noexcept {
        write_null_meta(oid_map, field, result);
    }

    static constexpr void write_meta(const oid_map_type& oid_map, std::size_t field, std::nullopt_t, impl_type& result) noexcept {
        write_null_meta(oid_map, field, result);
    }

    template <class T>
    static constexpr void write_meta(const oid_map_type& oid_map, std::size_t field, const std::reference_wrapper<T>& value, impl_type& result) {
       write_meta<std::decay_t<T>>(oid_map, field, value.get(), result);
    }

    template <class T>
    static constexpr is_nullable_t<T> write_meta(const oid_map_type& oid_map, std::size_t field, const T& value, impl_type& result) {
       if (is_null(value)) {
           write_typed_null_meta<std::decay_t<decltype(*value)>>(oid_map, field, result);
       } else {
           write_meta(oid_map, field, *value, result);
       }
    }

    template <class T>
    static constexpr void write_meta(const oid_map_type& oid_map, std::size_t field, const std::weak_ptr<T>& value, impl_type& result) {
       if (const auto locked = value.lock()) {
           write_meta(oid_map, field, *locked, result);
       } else {
           write_typed_null_meta<T>(oid_map, field, result);
       }
    }
};

template <class ... ArgsT>
using query = basic_query<std::allocator<char>, decltype(register_types<>()), ArgsT ...>;

template <class ... ArgsT>
auto make_query(const char* text, const ArgsT& ... args) {
    return query<const ArgsT& ...>(text, register_types<>(), args ...);
}

template <class OidMapT, class ... ArgsT>
auto make_query(const char* text, const not_nullopt_t<OidMapT>& oid_map, const ArgsT& ... args) {
    return basic_query<std::allocator<char>, OidMapT, const ArgsT& ...>(text, oid_map, args ...);
}

template <class BufferAllocatorT, class OidMapT, class ... ArgsT>
auto make_query(const char* text, const not_nullopt_t<OidMapT>& oid_map, const ArgsT& ... args) {
    return basic_query<BufferAllocatorT, OidMapT, const ArgsT& ...>(text, oid_map, args ...);
}

template <class BufferAllocatorT, class OidMapT, class ... ArgsT>
auto make_query(const BufferAllocatorT& buffer_allocator, const char* text, const not_nullopt_t<OidMapT>& oid_map, const ArgsT& ... args) {
    return basic_query<BufferAllocatorT, OidMapT, const ArgsT& ...>(buffer_allocator, text, oid_map, args ...);
}

} // namespace libapq
