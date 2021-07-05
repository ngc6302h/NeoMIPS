#include <cstring>

#include "util.hpp"
#include "constraints.hpp"
#include "error.hpp"

namespace NeoMIPS
{
	int64_t to_integer(const char* str, IntBase base)
	{
		try
		{
			switch (base)
			{
			case IntBase::decimal:
				return std::stoull(str, nullptr, 10);
			case IntBase::hex:
				return std::stoull(str, nullptr, 16);
			case IntBase::any:
				return std::stoull(str, nullptr, strchr(str, 'x') || strchr(str, 'X') ? 16 : 10);
			}
		}
		catch (const std::exception& e)
		{
			throw Error::IntegerParsingException("unknown", std::string("Failed to read input as number: ").append(str).append(". Inner exception: ").append(e.what()));
		}
	}

	float to_float(const std::string& str)
	{
		return std::strtof(str.c_str(), nullptr);
	}

	double to_double(const std::string& str)
	{
		return std::strtod(str.c_str(), nullptr);
	}
}