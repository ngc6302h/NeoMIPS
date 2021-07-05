#pragma once
#include <string>
#include "types.hpp"
#include "constraints.hpp"
namespace NeoMIPS
{
	template<typename UnaryPredicate, typename BinaryPredicate, typename T, typename... Rest>
	constexpr auto for_all(UnaryPredicate predicate, BinaryPredicate binPredicate, T first)
	{
		return predicate(first);
	}

	template<typename UnaryPredicate, typename BinaryPredicate, typename T, typename... Rest>
	constexpr auto for_all(UnaryPredicate predicate, BinaryPredicate binPredicate, T first, Rest... rest)
	{
		return binPredicate(predicate(first), for_all(predicate, binPredicate, rest...));
	}

	template<Char T>
	consteval const char* GetEncodingNameFromCharType()
	{
		if constexpr (std::is_same<T, char8_t>::value)
			return "UTF-8";
		else if constexpr (std::is_same<T, char16_t>::value)
			return "UTF-16";
		else return "UTF-32";
	}

	int64_t to_integer(const char* str, IntBase base = IntBase::any);
	float to_float(const std::string& str);
	double to_double(const std::string& str);

}