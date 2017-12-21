#pragma once

#include <apq/detail/pg_type.h>

#include <boost/hana/at_key.hpp>
#include <boost/hana/insert.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/type.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/optional.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
// Fusion adaptors support
#include <boost/fusion/support/is_sequence.hpp>
#include <boost/fusion/include/is_sequence.hpp>

#include <memory>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <type_traits>

namespace libapq {

namespace hana = ::boost::hana;
using namespace hana::literals;
/**
* PostgreSQL OID type - object identificator
*/
using oid_t = int32_t;

/**
* Type for non initialized OID
*/
using null_oid_t = std::integral_constant<oid_t, 0>;
constexpr null_oid_t null_oid;

/**
* Indicates if type is nullable. By default - type is not
* nullable.
*/
template <typename T>
struct is_nullable : ::std::false_type {};

/**
* These next types are nullable
*/
template <typename T>
struct is_nullable<::boost::optional<T>> : ::std::true_type {};
template <typename T>
struct is_nullable<::std::optional<T>> : ::std::true_type {};
template <typename T>
struct is_nullable<::boost::scoped_ptr<T>> : ::std::true_type {};
template <typename T, typename Deleter>
struct is_nullable<::std::unique_ptr<T, Deleter>> : ::std::true_type {};
template <typename T>
struct is_nullable<::boost::shared_ptr<T>> : ::std::true_type {};
template <typename T>
struct is_nullable<::std::shared_ptr<T>> : ::std::true_type {};
template <typename T>
struct is_nullable<::boost::weak_ptr<T>> : ::std::true_type {};
template <typename T>
struct is_nullable<::std::weak_ptr<T>> : ::std::true_type {};

template <typename T>
inline auto is_null(const T& v) noexcept ->
    std::enable_if_t<is_nullable<std::decay_t<T>>::value, bool> {
    return !v;
}

template <typename T>
inline auto is_null(const ::boost::weak_ptr<T>& v) noexcept {
    return is_null(v.lock());
}

template <typename T>
inline auto is_null(const ::std::weak_ptr<T>& v) noexcept {
    return is_null(v.lock());
}

template <typename T>
constexpr auto is_null(const T&) noexcept ->
    std::enable_if_t<!is_nullable<std::decay_t<T>>::value, std::false_type> {
    return {};
}

/**
* Indicates if type is an array
*/
template <typename T>
struct is_array : ::std::false_type {};

template <typename T, typename Alloc>
struct is_array<::std::vector<T, Alloc>> : ::std::true_type {};
template <typename T, typename Alloc>
struct is_array<::std::list<T, Alloc>> : ::std::true_type {};

/**
* Helper indicates if type is string
*/
template <typename T>
struct is_string : std::false_type {};

template <typename C, typename T, typename A>
struct is_string<std::basic_string<C, T, A>> : std::true_type {};

/**
* Indicates if type is adapted for introspection with Boost.Fusion
*/
template <typename T>
using is_fusion_adapted = std::integral_constant<bool, 
    ::boost::fusion::traits::is_sequence<T>::value
>;

/**
* Indicates if type is adapted for introspection with Boost.Hana
*/
template <typename T>
using is_hana_adapted = std::integral_constant<bool,
    ::boost::hana::Sequence<T>::value ||
    ::boost::hana::Struct<T>::value
>;

/**
* Indicates if type is a composite. In general we suppose that composite 
* is a type being adapted for introspection via Boost.Fusion or Boost.Hana,
* including tuples and compile-time sequences.
*/
template <typename T>
struct is_composite : std::integral_constant<bool,
    is_hana_adapted<T>::value ||
    is_fusion_adapted<T>::value
> {};


/**
* Type traits template forward declaration. Type traits contains information
* related to it's representation in the database. There are two different
* kind of traits - built-in types with constant OIDs and custom types with
* database depent OIDs. The functions below describe neccesary traits.
* For built-in types traits will be defined there. For custom types user
* must define traits.
*/
template <typename T>
struct type_traits;

/**
* Helpers to make size trait constant
*     bytes - makes fixed size trait
*     dynamic_size - makes dynamis size trait
*/
template <std::size_t n>
using bytes = std::integral_constant<std::size_t, n>;
using dynamic_size = void;

/**
* Helper defines the way for the type traits definitions.
* Type is undefined then Name type is not defined.
*     Name - type which can be converted into a string representation
*         which contain the fully qualified type name in DB
*     Oid - oid type - provides type's oid in database, may be defined for
*         built-in types only; custom types have only dynamic oid, depended
*         from the current state of DB.
*     Size - size type - provides information about type's object size, may
*         be specified only if the type's object has fixed size.
*/
template <typename Name, typename Oid = null_oid_t, typename Size = dynamic_size>
struct type_traits_helper {
    using name = Name;
    using oid = Oid;
    using size = Size;
};

/**
* By default all non specialized types have unspecified name, name
* oid and size.
*/
template <typename T>
struct type_traits : type_traits_helper<void> {};

/**
* Condition indicates if the specified type is built-in for PG
*/
template <typename T>
struct is_built_in : std::integral_constant<bool,
    !std::is_same_v<typename type_traits<T>::oid, null_oid_t>> {};

template <typename T>
using is_dynamic_size = std::is_same<typename type_traits<T>::size, dynamic_size>;

/**
* Function returns type name in Postgre SQL. For custom types user must
* define this name.
*/
template <typename T>
constexpr auto type_name(const T&) noexcept {
    constexpr auto name = typename type_traits<std::decay_t<T>>::name{};
    constexpr char const* retval = hana::to<char const*>(name);
    return retval;
}

/**
* Function returns object size.
*/
template <typename T>
inline auto size_of(const T&) noexcept  -> typename std::enable_if<
        !is_dynamic_size<std::decay_t<T>>::value,
        typename type_traits<std::decay_t<T>>::size>::type {
    return {};
}

template <typename T>
inline auto size_of(const T& v) noexcept -> typename std::enable_if<
        is_dynamic_size<std::decay_t<T>>::value,
        decltype(std::declval<T>().size())>::type {
    return v.size();
}

} // namespace libapq

#define LIBAPQ__TYPE_NAME_TYPE(Name) decltype(Name##_s)

#define LIBAPQ_PG_DEFINE_TYPE(Type, Name, Oid, Size) \
    namespace libapq {\
        template <>\
        struct type_traits<Type> : type_traits_helper<\
            LIBAPQ__TYPE_NAME_TYPE(Name), \
            std::integral_constant<oid_t, Oid>, \
            Size\
        >{};\
    }

#define LIBAPQ_PG_DEFINE_CUSTOM_TYPE(Type, Name, Size) \
    namespace libapq {\
        template <>\
        struct type_traits<Type> : type_traits_helper<\
            LIBAPQ__TYPE_NAME_TYPE(Name), \
            null_oid_t, \
            Size\
        >{};\
    }

LIBAPQ_PG_DEFINE_TYPE(bool, "bool", BOOLOID, bytes<1>)
LIBAPQ_PG_DEFINE_TYPE(char, "char", CHAROID, bytes<1>)
LIBAPQ_PG_DEFINE_TYPE(std::vector<char>, "bytea", BYTEAOID, dynamic_size)

LIBAPQ_PG_DEFINE_TYPE(boost::uuids::uuid, "uuid", UUIDOID, bytes<16>)
LIBAPQ_PG_DEFINE_TYPE(std::vector<boost::uuids::uuid>, "uuid[]", 2951, dynamic_size)

LIBAPQ_PG_DEFINE_TYPE(int64_t, "int8", INT8OID, bytes<8>)
LIBAPQ_PG_DEFINE_TYPE(std::vector<int64_t>, "int8[]", 1016, dynamic_size)
LIBAPQ_PG_DEFINE_TYPE(int32_t, "int4", INT4OID, bytes<4>)
LIBAPQ_PG_DEFINE_TYPE(std::vector<int32_t>, "int4[]", INT4ARRAYOID, dynamic_size)
LIBAPQ_PG_DEFINE_TYPE(int16_t, "int2", INT2OID, bytes<2>)
LIBAPQ_PG_DEFINE_TYPE(std::vector<int16_t>, "int2[]", INT2ARRAYOID, dynamic_size)

LIBAPQ_PG_DEFINE_TYPE(double, "float8", FLOAT8OID, bytes<8>)
LIBAPQ_PG_DEFINE_TYPE(std::vector<double>, "float8[]", 1022, dynamic_size)
LIBAPQ_PG_DEFINE_TYPE(float, "float4", FLOAT4OID, bytes<4>)
LIBAPQ_PG_DEFINE_TYPE(std::vector<float>, "float4[]", FLOAT4ARRAYOID, dynamic_size)

LIBAPQ_PG_DEFINE_TYPE(std::string, "text", TEXTOID, dynamic_size)
LIBAPQ_PG_DEFINE_TYPE(std::vector<std::string>, "text[]", TEXTARRAYOID, dynamic_size)


namespace libapq {

template <typename ... T>
constexpr decltype(auto) register_types() noexcept {
    return hana::make_map(
        hana::make_pair(hana::type_c<T>, null_oid())...
    );
}

using empty_oid_map = decltype(register_types<>());

/**
* Function sets oid for type in oid map.
*/
template <typename T, typename Map>
inline void set_type_oid(Map&& map, oid_t oid) noexcept {
    static_assert(!is_built_in<T>::value, "type must not be built-in");
    map[hana::type_c<T>] = oid;
}

/**
* Function returns oid for type from oid map.
*/
template <typename T, typename Map>
inline auto type_oid(const Map& map) noexcept
        -> std::enable_if_t<!is_built_in<T>::value, oid_t> {
    return map[hana::type_c<T>];
}

template <typename T, typename Map>
constexpr auto type_oid(const Map&) noexcept
        -> std::enable_if_t<is_built_in<T>::value, oid_t> {
    return typename type_traits<T>::oid();
}

template <typename T, typename Map>
inline auto type_oid(const Map& map, const T&) noexcept{
    return type_oid<std::decay_t<T>>(map);
}

/**
* Function returns true if type can be obtained from DB response with
* specified oid.
*/
template <typename T, typename Map>
inline bool accepts_oid(const Map& map, const T& v, oid_t oid) noexcept {
    return type_oid(map, v) == oid;
}

} // namespace libapq