#ifndef TINYREFL_BACKEND_HPP
#define TINYREFL_BACKEND_HPP

#include <ctti/detail/meta.hpp>
#include <ctti/detail/cstring.hpp>
#include <ctti/detail/hash.hpp>
#include <ctti/name.hpp>
#include <ctti/symbol.hpp>
#include <ctti/detail/algorithm.hpp>
#include <stdexcept>
#include <initializer_list>
#include <tinyrefl/utils/typename.hpp>

namespace tinyrefl
{

namespace meta = ctti::meta;

namespace backend
{

using type_hash_t = ctti::detail::hash_t;

template<ctti::detail::hash_t Hash>
using hash_constant = CTTI_STATIC_VALUE(Hash);

constexpr ctti::detail::cstring default_string_constant = "default tinyrefl string constant";

template<typename Hash>
constexpr ctti::detail::cstring string_constant()
{
    return default_string_constant;
}

struct no_metadata {};

template<typename Type>
struct metadata_of
{
    using type = no_metadata;
};

template<typename Type>
struct metadata_registered_for_type : public tinyrefl::meta::bool_<!std::is_same<
    typename metadata_of<Type>::type,
    no_metadata
>::value> {};

template<typename T>
using metadata_of_type = typename metadata_of<T>::type;

enum class entity_kind
{
    NAMESPACE,
    CLASS,
    ENUM,
    ENUM_VALUE,
    BASE_CLASS,
    MEMBER_VARIABLE,
    MEMBER_CLASS,
    MEMBER_FUNCTION,
    CONSTRUCTOR,
    MEMBER_ENUM,
    OBJECT
};

inline std::ostream& operator<<(std::ostream& os, const entity_kind e)
{
    switch(e)
    {
    case entity_kind::NAMESPACE:
        return os << "namespace";
    case entity_kind::CLASS:
        return os << "class";
    case entity_kind::ENUM:
        return os << "enum";
    case entity_kind::ENUM_VALUE:
        return os << "enum_value";
    case entity_kind::BASE_CLASS:
        return os << "base_class";
    case entity_kind::MEMBER_FUNCTION:
        return os << "member function";
    case entity_kind::CONSTRUCTOR:
        return os << "constructor";
    case entity_kind::MEMBER_VARIABLE:
        return os << "member variable";
    case entity_kind::MEMBER_CLASS:
        return os << "member class";
    case entity_kind::MEMBER_ENUM:
        return os << "member enum";
    case entity_kind::OBJECT:
        return os << "object";
    }

    return os;
}

template<typename T, std::size_t N>
struct constexpr_array
{
    constexpr const T& operator[](std::size_t i) const
    {
        return _array[i];
    }

    constexpr const T* begin() const
    {
        return ctti::detail::begin(_array);
    }

    constexpr const T* end() const
    {
        return ctti::detail::end(_array);
    }

    constexpr std::size_t size() const
    {
        return N;
    }

    T _array[N];

};

template<typename List>
struct typelist_to_array;

template<typename... Values>
struct typelist_to_array<tinyrefl::meta::list<Values...>>
{
    using value_type = typename std::remove_cv<decltype(tinyrefl::meta::pack_head_t<Values...>::value)>::type;
    using array_type = constexpr_array<value_type, ctti::detail::max(static_cast<std::size_t>(1), sizeof...(Values))>;

    static constexpr array_type value = {Values::value...};
};
template<typename Name, typename Namespace, typename FullAttribute, typename Args>
struct attribute_metadata;
template<typename Name, typename Namespace, typename FullAttribute, typename... Args>
struct typelist_to_array<attribute_metadata<Name, Namespace, FullAttribute, tinyrefl::meta::list<Args...>>>
{
	using value_type = typename std::remove_cv<decltype(tinyrefl::meta::pack_head_t<Name, Namespace, FullAttribute, Args...>::value)>::type;
	using array_type = constexpr_array<value_type, ctti::detail::max(static_cast<std::size_t>(1), sizeof...(Args)+3)>;

	static constexpr array_type value = { Name::value,Namespace::value,FullAttribute::value, Args::value... };
};

template<typename... Values>
constexpr typename typelist_to_array<tinyrefl::meta::list<Values...>>::array_type typelist_to_array<tinyrefl::meta::list<Values...>>::value;

template<typename T>
struct array_view
{
    constexpr array_view(const T* begin, const T* end) :
        _begin{begin}, _end{end}
    {}

    template<std::size_t N>
    constexpr array_view(const T (&array)[N]) :
        array_view{ctti::detail::begin(array), ctti::detail::end(array)}
    {}

    template<std::size_t N>
    constexpr array_view(const constexpr_array<T, N>& array) :
        array_view{array.begin(), array.end()}
    {}

    constexpr std::size_t size() const
    {
        return static_cast<std::size_t>(_end - _begin);
    }

    constexpr const T* begin() const
    {
        return _begin;
    }

    constexpr const T* end() const
    {
        return _end;
    }

    constexpr array_view operator()(std::size_t begin, std::size_t end) const
    {
        return {_begin + begin, _begin + end};
    }

    constexpr array_view trim(std::size_t n) const
    {
        return (*this)(0, size() - n);
    }

    constexpr const T& operator[](std::size_t i) const
    {
        return _begin[i];
    }

private:
    const T* _begin;
    const T* _end;
};

template<typename T, std::size_t N>
constexpr array_view<T> make_array_view(const T(&array)[N])
{
    return {array};
}

template<typename T, std::size_t N>
constexpr array_view<T> make_array_view(const constexpr_array<T, N>& array)
{
    return {array};
}

struct attribute
{
    ctti::name_t name;
    ctti::name_t namespace_;
    ctti::detail::cstring full_attribute;
    array_view<ctti::detail::cstring> args;

    constexpr attribute(const ctti::name_t& name, const ctti::name_t& namespace_, const ctti::detail::cstring& full_attribute, const array_view<ctti::detail::cstring>& args) :
        name{name},
        namespace_{namespace_},
        full_attribute{full_attribute},
        args{args}
    {}

    friend constexpr bool operator==(const attribute& lhs, const ctti::detail::cstring& name)
    {
        return name == lhs.name.full_name();
    }

    friend constexpr bool operator==(const attribute& lhs, const attribute& rhs)
    {
        return lhs.name.full_name() == rhs.name.full_name();
    }

    friend constexpr bool operator!=(const attribute& lhs, const ctti::detail::cstring& name)
    {
        return !(lhs == name);
    }

    friend constexpr bool operator!=(const attribute& lhs, const attribute& rhs)
    {
        return !(lhs == rhs);
    }
};

template<typename Name, typename Namespace, typename FullAttribute, typename Args>
struct attribute_metadata;

template<typename Name, typename Namespace, typename FullAttribute, typename... Args>
struct attribute_metadata<Name, Namespace, FullAttribute, tinyrefl::meta::list<Args...>>
    : public attribute
{
    static constexpr ctti::name_t name = tinyrefl::backend::string_constant<Name>();
    static constexpr ctti::name_t namespace_ = tinyrefl::backend::string_constant<Namespace>();
    static constexpr ctti::detail::cstring full_attribute = tinyrefl::backend::string_constant<FullAttribute>();
    static constexpr ctti::detail::cstring args[] = {tinyrefl::backend::string_constant<Args>()..., ""};
    static constexpr attribute value{name, namespace_, full_attribute, make_array_view(args).trim(1)};

    constexpr attribute_metadata() :
        attribute{name, namespace_, full_attribute, make_array_view(args).trim(1)}
    {}
};

template<typename Name, typename Namespace, typename FullAttribute, typename... Args>
constexpr ctti::name_t attribute_metadata<Name, Namespace, FullAttribute, tinyrefl::meta::list<Args...>>::name;

template<typename Name, typename Namespace, typename FullAttribute, typename... Args>
constexpr ctti::name_t attribute_metadata<Name, Namespace, FullAttribute, tinyrefl::meta::list<Args...>>::namespace_;

template<typename Name, typename Namespace, typename FullAttribute, typename... Args>
constexpr ctti::detail::cstring attribute_metadata<Name, Namespace, FullAttribute, tinyrefl::meta::list<Args...>>::full_attribute;

template<typename Name, typename Namespace, typename FullAttribute, typename... Args>
constexpr ctti::detail::cstring attribute_metadata<Name, Namespace, FullAttribute, tinyrefl::meta::list<Args...>>::args[];

using dummy_attribute_metadata = attribute_metadata<hash_constant<0>, hash_constant<0>, hash_constant<0>, tinyrefl::meta::list<>>;

template<typename T, typename U>
constexpr const T* find(const T* begin, const T* end, const U& value)
{
    return begin != end ? (*begin == value ? begin : find(++begin, end, value)) : end;
}

template<typename T, typename U>
constexpr const T* find(array_view<T> range, const U& value)
{
    return find(range.begin(), range.end(), value);
}

template<typename T, std::size_t N, typename U>
constexpr const T* find(const constexpr_array<T, N>& array, const U& value)
{
    return find(make_array_view(array), value);
}

template<typename Metadata>
constexpr const attribute* find_attribute(const Metadata&, const ctti::detail::cstring& attribute)
{
    return find(Metadata::attributes, attribute);
}

template<typename Metadata>
constexpr bool has_attribute(const Metadata& metadata, const ctti::detail::cstring& attribute)
{
    return find_attribute(metadata, attribute) != Metadata::attributes.end();
}

template<typename Attributes>
using attributes_plus_dummy = ctti::meta::append_t<Attributes, dummy_attribute_metadata>;

template<typename Attributes>
struct metadata_with_attributes
{
    static constexpr decltype(typelist_to_array<attributes_plus_dummy<Attributes>>::value) attributes = typelist_to_array<attributes_plus_dummy<Attributes>>::value;

    constexpr bool has_attribute(const ctti::detail::cstring& name) const
    {
        return tinyrefl::backend::has_attribute(*this, name);
    }

    constexpr const attribute& get_attribute(const ctti::detail::cstring& name) const
    {
        return *tinyrefl::backend::find_attribute(*this, name);
    }

    constexpr const attribute& get_attribute(std::size_t i) const
    {
        return get_attributes()[i];
    }

    constexpr array_view<attribute> get_attributes() const
    {
        return array_view<attribute>{attributes}.trim(1);
    }
};

template<typename Attributes>
constexpr decltype(typelist_to_array<attributes_plus_dummy<Attributes>>::value) metadata_with_attributes<Attributes>::attributes;

using dummy_metadata_with_attributes = metadata_with_attributes<tinyrefl::meta::list<>>;

template<typename Name, typename Pointer, typename Attributes>
struct member : public Pointer, public metadata_with_attributes<Attributes>
{
    using pointer_type = typename Pointer::value_type;
    using pointer_static_value = Pointer;
    static constexpr entity_kind kind = (std::is_member_function_pointer<pointer_type>::value ? entity_kind::MEMBER_FUNCTION : entity_kind::MEMBER_VARIABLE);
    static constexpr ctti::name_t name = tinyrefl::backend::string_constant<Name>();

    constexpr member() = default;

    constexpr pointer_type get() const
    {
        return Pointer::value;
    }

    template<typename Class>
    constexpr auto get(const Class& object) const -> decltype(object.*Pointer::value)
    {
        return object.*get();
    }

    template<typename Class>
    constexpr auto get(Class& object) const -> decltype(object.*Pointer::value)
    {
        return object.*get();
    }

    template<typename Class, typename... Args>
    constexpr auto get(const Class& object, Args&&... args) const -> decltype((object.*Pointer::value)(std::forward<Args>(args)...))
    {
        return (object.*get())(std::forward<Args>(args)...);
    }

    template<typename Class, typename... Args>
    constexpr auto get(Class& object, Args&&... args) const -> decltype((object.*Pointer::value)(std::forward<Args>(args)...))
    {
        return (object.*get())(std::forward<Args>(args)...);
    }
};

template<typename Name, typename Pointer, typename Attributes>
constexpr ctti::name_t member<Name, Pointer, Attributes>::name;

template<typename Name, typename Class, typename Args, typename Attributes = tinyrefl::meta::list<>>
struct constructor;

template<typename Name, typename Class, typename... Args, typename Attributes>
struct constructor<Name, Class, tinyrefl::meta::list<Args...>, Attributes> : public metadata_with_attributes<Attributes>
{
    using class_type = Class;
    using args = tinyrefl::meta::list<Args...>;
    using decayed_args = tinyrefl::meta::list<tinyrefl::meta::decay_t<Args>...>;

    static constexpr ctti::detail::cstring name = tinyrefl::backend::string_constant<Name>();
    static constexpr entity_kind kind = entity_kind::CONSTRUCTOR;

    constexpr constructor() = default;

    constexpr auto construct(Args... args) const -> tinyrefl::meta::enable_if_t<
        std::is_move_constructible<Class>::value || std::is_copy_constructible<Class>::value,
        Class
    >
    {
        return Class{std::forward<Args>(args)...};
    }

    void inplace_construct(void* where, Args... args) const
    {
        new(where) Class{std::forward<Args>(args)...};
    }

    void inplace_construct(Class* where, Args... args) const
    {
        new(where) Class{std::move(args)...};
    }
};

template<typename Name, typename Class, typename... Args, typename Attributes>
constexpr ctti::detail::cstring constructor<Name, Class, tinyrefl::meta::list<Args...>, Attributes>::name;

template<
    typename Name,
    typename Class,
    typename BaseClasses,
    typename Constructors,
    typename MemberFunctions,
    typename MemberVariables,
    typename Classes,
    typename Enums,
    typename Attributes = tinyrefl::meta::list<>
>
struct class_ : public metadata_with_attributes<Attributes>
{
private:
    template<entity_kind Kind>
    struct has_kind
    {
        template<typename Member>
        struct apply
        {
            using type = tinyrefl::meta::bool_<Member::kind == Kind>;
        };
    };

public:
    static constexpr ctti::name_t name = tinyrefl::backend::string_constant<Name>();
    static constexpr entity_kind kind = entity_kind::CLASS;
    using class_type = Class;
    using base_classes = BaseClasses;
    using constructors = Constructors;
    using member_functions = MemberFunctions;
    using member_variables = MemberVariables;
    using members = tinyrefl::meta::cat_t<member_functions, member_variables>;
    using classes = Classes;
    using enums = Enums;

    constexpr class_() = default;

private:
    template<typename Total, typename BaseClass,
        typename HasMetadata = tinyrefl::meta::bool_<metadata_registered_for_type<BaseClass>::value>>
    struct accumulate
    {
        using type = Total;
    };

    template<typename Total, typename BaseClass>
    struct accumulate<Total, BaseClass, std::true_type>
    {
        using type = tinyrefl::meta::size_t<Total::value + metadata_of_type<BaseClass>::members::size + metadata_of_type<BaseClass>::total_members::value>;
    };

public:
    using total_base_members = tinyrefl::meta::foldl_t<tinyrefl::meta::defer<accumulate>, tinyrefl::meta::size_t<0>, base_classes>;
    using total_members = tinyrefl::meta::size_t<members::size + total_base_members::value>;
};

template<
    typename Name,
    typename Class,
    typename BaseClasses,
    typename Constructors,
    typename MemberFunctions,
    typename MemberVariables,
    typename Classes,
    typename Enums,
    typename Attributes
>
constexpr ctti::name_t class_<Name, Class, BaseClasses, Constructors, MemberFunctions, MemberVariables, Classes, Enums, Attributes>::name;

template<typename Name, typename Value, typename Attributes = tinyrefl::meta::list<>>
struct enum_value : public Value, public metadata_with_attributes<Attributes>
{
    using value_type = typename Value::value_type;
    using underlying_value_type = typename std::underlying_type<value_type>::type;
    using value_static_value = Value;
    static constexpr entity_kind kind = entity_kind::ENUM_VALUE;
    static constexpr ctti::name_t name = tinyrefl::backend::string_constant<Name>();

    constexpr enum_value() = default;

    constexpr ctti::detail::cstring value_name() const
    {
        return name.name();
    }

    constexpr ctti::name_t full_value_name() const
    {
        return name;
    }

    constexpr value_type value() const
    {
        return Value::value;
    }

    constexpr underlying_value_type underlying_value() const
    {
        return static_cast<underlying_value_type>(value());
    }

    friend constexpr bool operator==(const enum_value& lhs, const ctti::detail::cstring& name)
    {
        return lhs.value_name() == name;
    }

    friend constexpr bool operator==(const enum_value& lhs, const value_type value)
    {
        return lhs.value() == value;
    }

    friend constexpr bool operator==(const enum_value& lhs, const underlying_value_type value)
    {
        return lhs.underlying_value() == value;
    }

    friend constexpr bool operator!=(const enum_value& lhs, const ctti::detail::cstring& name)
    {
        return !(lhs == name);
    }

    friend constexpr bool operator!=(const enum_value& lhs, const value_type value)
    {
        return !(lhs == value);
    }

    friend constexpr bool operator!=(const enum_value& lhs, const underlying_value_type value)
    {
        return !(lhs == value);
    }
};

template<typename Name, typename Value, typename Attributes>
constexpr ctti::name_t enum_value<Name, Value, Attributes>::name;

template<typename Name, typename Enum, typename Values, typename Attributes = tinyrefl::meta::list<>>
struct enum_;

template<typename Name, typename Enum, typename... Values, typename Attributes>
struct enum_<Name, Enum, tinyrefl::meta::list<Values...>, Attributes> : public metadata_with_attributes<Attributes>
{
    static constexpr entity_kind kind = entity_kind::ENUM;
    static constexpr ctti::name_t enum_name = tinyrefl::backend::string_constant<Name>();
    using values = tinyrefl::meta::list<Values...>;
    using enum_type = Enum;
    using underlying_type = typename std::underlying_type<enum_type>::type;

    struct value_t
    {
        template<typename EnumValueMetadata>
        constexpr value_t(const EnumValueMetadata& enum_value) :
            value_t{enum_value.value(), enum_value.value_name(), enum_value.get_attributes()}
        {}

        constexpr value_t(const enum_type value, const ctti::detail::cstring& name, const array_view<attribute>& attributes) :
            _value{value},
            _name{name},
            _attributes{attributes}
        {}

        constexpr ctti::detail::cstring name() const
        {
            return _name;
        }

        constexpr enum_type value() const
        {
            return _value;
        }

        constexpr underlying_type underlying_value() const
        {
            return static_cast<underlying_type>(value());
        }

        constexpr bool has_attribute(const ctti::detail::cstring& name) const
        {
            return tinyrefl::backend::find(_attributes, name) != _attributes.end();
        }

        constexpr const attribute& get_attribute(const ctti::detail::cstring& name) const
        {
            return *tinyrefl::backend::find(_attributes, name);
        }

        constexpr operator enum_type() const
        {
            return value();
        }

        friend constexpr bool operator==(const value_t& lhs, const ctti::detail::cstring& name)
        {
            return lhs.value_name() == name;
        }

        friend constexpr bool operator==(const value_t& lhs, const enum_type value)
        {
            return lhs.value() == value;
        }

        friend constexpr bool operator==(const value_t& lhs, const underlying_type value)
        {
            return lhs.underlying_value() == value;
        }

        friend constexpr bool operator!=(const value_t& lhs, const ctti::detail::cstring& name)
        {
            return !(lhs == name);
        }

        friend constexpr bool operator!=(const value_t& lhs, const enum_type value)
        {
            return !(lhs == value);
        }

        friend constexpr bool operator!=(const value_t& lhs, const underlying_type value)
        {
            return !(lhs == value);
        }

        inline friend std::ostream& operator<<(std::ostream& os, const value_t value)
        {
            return os << value.name();
        }

    private:
        enum_type _value;
        ctti::detail::cstring _name;
        array_view<attribute> _attributes;
    };

    using values_array = std::array<value_t, values::size>;
    static constexpr values_array enum_values = {{value_t{Values{}}...}};
    static constexpr value_t invalid_value = value_t{enum_type{}, "invalid enum value", dummy_metadata_with_attributes{}.get_attributes()};

    constexpr enum_() = default;

    constexpr std::size_t count() const
    {
        return values::size;
    }

    constexpr const ctti::name_t& name() const
    {
        return enum_::enum_name;
    }

    constexpr const value_t& get_value(const ctti::detail::cstring& name) const
    {
        return get_values()[find_value_index(name)];
    }

    constexpr const value_t& get_value(const std::size_t i) const
    {
        return get_values()[i];
    }

    constexpr const value_t& get_value(const enum_type value) const
    {
        return get_values()[find_value_index(value)];
    }

    constexpr const value_t& get_value(const underlying_type value) const
    {
        return get_value(static_cast<enum_type>(value));
    }

    constexpr bool is_enumerated_value(const underlying_type value) const
    {
        return find_value_index(value) >= 0;
    }

    constexpr bool is_enumerated_value(const ctti::detail::cstring name) const
    {
        return find_value_index(name) >= 0;
    }

    constexpr const values_array& get_values() const
    {
        return enum_values;
    }

    using const_iterator = typename values_array::const_iterator;

    constexpr const_iterator begin() const
    {
        return get_values().begin();
    }

    constexpr const_iterator end() const
    {
        return get_values().end();
    }

private:
    constexpr int find_value_index(const ctti::detail::cstring& name, std::size_t i = 0) const
    {
        return (i < count()) ?
            (get_value(i).name() == name ?
                static_cast<int>(i) :
                find_value_index(name, i + 1))
            : -1;
    }

    constexpr int find_value_index(const underlying_type value, std::size_t i = 0) const
    {
        return (i < count()) ?
            (get_values()[i] == value ?
                static_cast<int>(i) :
                find_value_index(value, i + 1))
            : -1;
    }

    constexpr int find_value_index(const enum_type value, std::size_t i = 0) const
    {
        return (i < count()) ?
            (get_values()[i] == value ?
                static_cast<int>(i) :
                find_value_index(value, i + 1))
            : -1;
    }
};

template<typename Name, typename Enum, typename... Values, typename Attributes>
constexpr typename enum_<Name, Enum, tinyrefl::meta::list<Values...>, Attributes>::values_array enum_<Name, Enum, tinyrefl::meta::list<Values...>, Attributes>::enum_values;
template<typename Name, typename Enum, typename... Values, typename Attributes>
constexpr ctti::name_t enum_<Name, Enum, tinyrefl::meta::list<Values...>, Attributes>::enum_name;
template<typename Name, typename Enum, typename... Values, typename Attributes>
constexpr typename enum_<Name, Enum, tinyrefl::meta::list<Values...>, Attributes>::value_t enum_<Name, Enum, tinyrefl::meta::list<Values...>, Attributes>::invalid_value;

}

}

#ifndef TINYREFL_PP_CAT
#define TINYREFL_PP_CAT_IMPL(x, y) x ## y
#define TINYREFL_PP_CAT(x, y) TINYREFL_PP_CAT_IMPL(x, y)
#endif // TINYREFL_PP_CAT

#ifndef TINYREFL_PP_STR
#define TINYREFL_PP_STR(...) #__VA_ARGS__
#endif // TINYREFL_PP_STR

#ifndef TINYREFL_PP_UNWRAP
#define TINYREFL_PP_UNWRAP(...) __VA_ARGS__
#endif // TINYREFL_PP_UNWRAP

#define TINYREFL_DEFINE_STRINGS
#define TINYREFL_STRING(...) CTTI_STATIC_VALUE(ctti::detail::cstring{TINYREFL_PP_STR(__VA_ARGS__)}.hash())
#define TINYREFL_DEFINE_STRING(...)                                                               \
    namespace tinyrefl { namespace backend {                                                               \
        template<> \
        constexpr ::ctti::detail::cstring string_constant<CTTI_STATIC_VALUE(ctti::detail::cstring{TINYREFL_PP_STR(__VA_ARGS__)}.hash())>() { \
            return ::ctti::detail::cstring{TINYREFL_PP_STR(__VA_ARGS__)}; \
        } \
    } /* namespace backend */ } // namespace tinyrefl

#define TINYREFL_SEQUENCE(elems) ::tinyrefl::meta::list<TINYREFL_PP_UNWRAP elems>
#define TINYREFL_TYPE(name, fullname) fullname
#define TINYREFL_VALUE(type, value) ::ctti::static_value<type, value>
#define TINYREFL_ATTRIBUTE(name, namespace_, full_attribute, args) ::tinyrefl::backend::attribute_metadata<name, namespace_, full_attribute, args>
#define TINYREFL_CONSTRUCTOR(name, fullname, class_type, signature, attributes) ::tinyrefl::backend::constructor<name, class_type, signature, attributes>
#define TINYREFL_MEMBER_FUNCTION(name, fullname, parent_class_type, return_type, signature, pointer, attributes) ::tinyrefl::backend::member<fullname, pointer, attributes>
#define TINYREFL_MEMBER_VARIABLE(name, fullname, parent_class_type, value_type, pointer, attributes) ::tinyrefl::backend::member<fullname, pointer, attributes>
#define TINYREFL_ENUM_VALUE(name, fullname, type, value, attributes) ::tinyrefl::backend::enum_value<fullname, value, attributes>

#define TINYREFL_REFLECT_MEMBER(member)                  \
    namespace tinyrefl { namespace backend {             \
    template<>                                           \
    struct metadata_of<typename member::pointer_static_value>               \
    {                                                    \
        using type = member;                            \
    };                                                   \
    } /* namespace backend */ } // namespace tinyrefl

#define TINYREFL_REFLECT_ENUM_VALUE(value)                  \
    namespace tinyrefl { namespace backend {             \
    template<>                                           \
    struct metadata_of<typename value::value_static_value>               \
    {                                                    \
        using type = value;                            \
    };                                                   \
    } /* namespace backend */ } // namespace tinyrefl

#define TINYREFL_REFLECT_CLASS(classname, classtype, bases, ctors, functions, variables, classes, enums, attributes) \
    namespace tinyrefl { namespace backend {               \
    template<>                                             \
    struct metadata_of<classtype> \
    {                                                      \
        using type = class_<classname, classtype, bases, ctors, functions, variables, classes, enums, attributes>;                  \
    };                                                     \
    } /* namespace backend */ } // namespace tinyrefl

#define TINYREFL_REFLECT_ENUM(name, enum_type, values, attributes)    \
    namespace tinyrefl { namespace backend {              \
    template<>                                            \
    struct metadata_of<enum_type> \
    {                                                     \
        using type = enum_<name, enum_type, values, attributes>; \
    };                                                    \
    } /* namespace backend */ } // namespace tinyrefl


#define TINYREFL_GODMODE                                                                                         \
    struct tinyrefl_godmode_tag {};                                                                              \
    template<                                                                                                    \
        typename __TinyRefl__GodModeTemplateParam__BaseClasses,                                                  \
        typename __TinyRelf__GodModeTemplateParam__Members,                                                      \
        typename __TinyRelf__GodModeTemplateParam__Classes,                                                      \
        typename __TinyRelf__GodModeTemplateParam__Enums,                                                        \
        typename __TinyRefl__GodModeTemplateParam__Attributes                                                    \
    >                                                                                                            \
    friend struct ::tinyrefl::backend::class_;                                                                   \
    template<typename __TinyRefl__GodModeTemplateParam__Type>                                                    \
    friend struct ::tinyrefl::backend::metadata_of;                                                              \
    template<typename __TinyRefl__GodModeTemplateParam__Name, typename __TinyRefl__GodModeTemplateParam__Pointer, typename __TinyRefl__GodModeTemplateParam__Attributes> \
    friend struct ::tinyrefl::backend::member;                                                                   \
    template<typename __TinyRefl__GodModeTemplateParam__Signature, typename __TinyRefl__GodModeTemplateParam__Attributes>                                              \
    friend struct ::tinyrefl::backend::constructor;                                                              \
    template<typename __TinyRefl__GodModeTemplateParam__Name, typename __TinyRefl__GodModeTemplateParam__Enum, typename __TinyRefl__GodModeTemplateParam__Values, typename __TinyRefl__GodModeTemplateParam__Attributes> \
    friend struct ::tinyrefl::backend::enum_;

#endif // TINYREFL_BACKEND_HPP
