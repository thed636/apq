#pragma once

#include <apq/query.h>
#include <apq/type_traits.h>

#include <boost/hana/fold.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/tuple.hpp>

namespace libapq {
namespace detail {

template <std::size_t, char ... c>
struct concat_query_element_result {
    using string_type = boost::hana::string<c ...>;

    constexpr string_type string() noexcept {
        return string_type();
    }
};

template <std::size_t n, char ... c>
auto make_concat_query_element_result(boost::hana::string<c ...>) noexcept {
    return concat_query_element_result<n, c ...>();
}

template <std::size_t value>
constexpr auto digit_to_string(std::integral_constant<std::size_t, value>) noexcept {
    return boost::hana::string_c<'0' + value>;
}

template <std::size_t value>
constexpr auto to_string() noexcept;

template <std::size_t value>
constexpr auto to_string_head(std::integral_constant<std::size_t, value>) noexcept {
    return to_string<value>();
}

constexpr auto to_string_head(std::integral_constant<std::size_t, 0>) noexcept {
    return boost::hana::string_c<>;
}

template <std::size_t value>
constexpr auto to_string() noexcept {
    constexpr auto divident = value % 10;
    return to_string_head(std::integral_constant<std::size_t, (value - divident) / 10>())
        + digit_to_string(std::integral_constant<std::size_t, divident>());
}

struct query_text_tag {};

struct query_param_tag {};

template <class ValueT, class TagT>
struct query_element {
    using value_type = ValueT;
    using tag_type = TagT;

    value_type value;
};

template <class T, std::size_t next_param_number, char ... c>
constexpr auto concat_query_text(concat_query_element_result<next_param_number, c ...>, const query_element<T, query_text_tag>&) noexcept {
    using s_type = concat_query_element_result<next_param_number, c ...>;
    return make_concat_query_element_result<next_param_number>(typename s_type::string_type() + T());
}

template <class T, std::size_t next_param_number, char ... c>
constexpr auto concat_query_text(concat_query_element_result<next_param_number, c ...>, const query_element<T, query_param_tag>&) noexcept {
    using namespace boost::hana::literals;
    using s_type = concat_query_element_result<next_param_number, c ...>;
    return make_concat_query_element_result<next_param_number + 1>(
        typename s_type::string_type() + "$"_s + to_string<next_param_number>()
    );
}

template <class T, class ... ValuesT>
constexpr auto concat_query_params(boost::hana::tuple<ValuesT ...>&& result, const query_element<T, query_text_tag>&) noexcept {
    return result;
}

template <class T, class ... ValuesT>
constexpr auto concat_query_params(boost::hana::tuple<ValuesT ...>&& result, const query_element<T, query_param_tag>& element) {
    return hana::concat(std::move(result), hana::make_tuple(element.value));
}

} // namespace detail

template <class ElementsT>
struct query_builder {
    using elements_type = ElementsT;

    elements_type elements;

    constexpr auto text() const noexcept {
        namespace hana = boost::hana;
        using namespace detail;
        return hana::fold(elements, make_concat_query_element_result<1>(hana::string_c<>),
            [] (auto s, const auto& v) { return concat_query_text(s, v); }).string();
    }

    constexpr auto params() const noexcept {
        namespace hana = boost::hana;
        using namespace detail;
        return hana::fold(elements, hana::tuple<>(),
            [] (auto&& s, const auto& v) { return concat_query_params(std::move(s), v); });
    }

    constexpr auto build() const {
        namespace hana = boost::hana;
        return hana::unpack(
            params(),
            [&] (const auto& ... params) {
                return make_query(hana::to<const char*>(text()), params ...);
            }
        );
    }

    template <class OidMapT>
    constexpr auto build(const OidMapT& oid_map) const {
        namespace hana = boost::hana;
        return hana::unpack(
            params(),
            [&] (const auto& ... params) {
                return make_query(hana::to<const char*>(text()), oid_map, params ...);
            }
        );
    }

    template <class BufferAllocatorT, class OidMapT>
    constexpr auto build(const BufferAllocatorT& buffer_allocator, const OidMapT& oid_map) const {
        namespace hana = boost::hana;
        return hana::unpack(
            params(),
            [&] (const auto& ... params) {
                return make_query(buffer_allocator, hana::to<const char*>(text()), oid_map, params ...);
            }
        );
        return ;
    }
};

template <char ... c>
constexpr auto make_query_builder(boost::hana::string<c ...>) {
    namespace hana = boost::hana;
    using namespace detail;
    using elements_tuple = hana::tuple<query_element<hana::string<c ...>, query_text_tag>>;
    return query_builder<elements_tuple> {elements_tuple()};
}

template <class ... ElementsT>
constexpr auto make_query_builder(boost::hana::tuple<ElementsT ...>&& elements) {
    namespace hana = boost::hana;
    using namespace detail;
    return query_builder<boost::hana::tuple<ElementsT ...>> {std::move(elements)};
}

template <class ElementsT, class OtherElementsT>
constexpr auto operator +(query_builder<ElementsT>&& builder, query_builder<OtherElementsT>&& other) {
    namespace hana = boost::hana;
    return make_query_builder(hana::concat(std::move(builder.elements), std::move(other.elements)));
}

template <class ValueT, class ElementsT>
constexpr auto operator +(query_builder<ElementsT>&& builder, ValueT&& value) {
    namespace hana = boost::hana;
    using namespace detail;
    using query_element_param = query_element<std::decay_t<ValueT>, query_param_tag>;
    using tail_tuple = hana::tuple<query_element_param>;
    return make_query_builder(hana::concat(std::move(builder.elements), tail_tuple(query_element_param {std::forward<ValueT>(value)})));
}

template <class ValueT, class ElementsT>
constexpr auto operator +(query_builder<ElementsT>&& builder, const ValueT& value) {
    namespace hana = boost::hana;
    using namespace detail;
    using query_element_param = query_element<std::reference_wrapper<const ValueT>, query_param_tag>;
    using tail_tuple = hana::tuple<query_element_param>;
    return make_query_builder(hana::concat(std::move(builder.elements), tail_tuple(query_element_param {value})));
}

namespace literals {

template <class CharT, CharT ... c>
constexpr auto operator "" _SQL() {
    namespace hana = boost::hana;
    return make_query_builder(hana::string<c ...>());
}

} // namespace literals
} // namespace libapq
