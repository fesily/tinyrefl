#ifndef CTTI_DETAIL_CSTRING_HPP
#define CTTI_DETAIL_CSTRING_HPP

#include "hash.hpp"
#include "algorithm.hpp"
#include <ostream>
#include <string>
#include <string_view>
namespace ctti
{

namespace detail
{

class cstring : public std::string_view
{
public:
	constexpr cstring(const std::string_view& s)
		:std::string_view{s}
	{
		
	}
    template<std::size_t N>
    constexpr cstring(const char (&str)[N]) :
        cstring{&str[0], N - 1}
    {}

    constexpr cstring(const char* begin, std::size_t length) :
		std::string_view{begin,length}
    {}

    constexpr cstring(const char* begin, const char* end) :
        cstring{begin, static_cast<std::size_t>(end - begin)}
    {}
	constexpr cstring()
    {
	    
    }
    constexpr cstring(const char* begin) :
		std::string_view{begin}
    {}

	using std::string_view::length;
	using std::string_view::size;

    constexpr hash_t hash() const
    {
        return fnv1a_hash(length(), begin());
    }
/*
    std::string cppstring() const
    {
        return {begin(), end()};
    }

    std::string str() const
    {
        return cppstring();
    }*/

    constexpr const char* begin() const
    {
		return this->data();
    }

    constexpr const char* end() const
    {
		return this->data() + length();
    }
	using std::string_view::operator[];

    constexpr const char* operator()(std::size_t i) const
    {
        return this->data() + i;
    }

    constexpr cstring operator()(std::size_t begin, std::size_t end) const
    {
        return { this->data() + begin, this->data() + end};
    }

    constexpr cstring pad(std::size_t begin_offset, std::size_t end_offset) const
    {
        return operator()(begin_offset, size() - end_offset);
    }

    friend std::ostream& operator<<(std::ostream& os, const cstring& str)
    {
		os << static_cast<std::string_view>(str);
        return os;
    }

};

constexpr bool operator==(const cstring& lhs, const cstring& rhs)
{
    return ctti::detail::equal_range(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

constexpr bool operator!=(const cstring& lhs, const cstring& rhs)
{
    return !(lhs == rhs);
}

}

}

#endif // CTTI_DETAIL_CSTRING_HPP
