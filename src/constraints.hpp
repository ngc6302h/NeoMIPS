#pragma once
#include <type_traits>

namespace NeoMIPS
{
	template <typename T>
	concept Char = std::is_same<char, T>::value || std::is_same<char8_t, T>::value
		|| std::is_same<char16_t, T>::value || std::is_same<char32_t, T>::value;
}