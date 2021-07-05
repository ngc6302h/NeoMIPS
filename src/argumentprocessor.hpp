#pragma once
#include <optional>
#include <memory>
#include <unordered_map>
#include <variant>
#include <cstring>
#include "option.hpp"
#include "types.hpp"
#include "util.hpp"

namespace NeoMIPS
{
	template<typename... Args>
	bool is_arg(const char* arg, Args... args)
	{
		return for_all([=](const char* a) -> bool {return strcmp(a, arg) == 0; }, [](auto a, auto b) -> bool {return a || b; }, args...);
	}

	class ArgumentProcessor
	{
		static void SetDefaultOptions(argmap_t& map);
	public:
		static void ReadArguments(int argc, char** argv, argmap_t& argMap);
	};
}