#ifndef TINYREFL_UTILS_TYPENAME_HPP
#define TINYREFL_UTILS_TYPENAME_HPP

#include <ctti/detail/language_features.hpp>

#ifdef CTTI_HAS_CONSTEXPR_PRETTY_FUNCTION
#include <ctti/nameof.hpp>

namespace tinyrefl
{

namespace utils
{

template<typename T>
constexpr std::string_view type_name()
{
    return ctti::nameof_v<T>;
}

}

}

#else
#include <type_info>
#include <tinyrefl/utils/demangle.hpp>

namespace tinyrefl
{

namespace utils
{

template<typename T>
const std::string& type_name()
{
    static std::string name{tinyrefl::utils::demangle(typeid(T).name())};
    return name;
}

}

}
#endif // CTTI_HAS_CONSTEXPR_PRETTY_FUNCTION

#endif // TINYREFL_UTILS_TYPENAME_HPP
