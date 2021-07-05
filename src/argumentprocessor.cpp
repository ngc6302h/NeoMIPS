#include <filesystem>
#include <iostream>
#include "argumentprocessor.hpp"
#include "util.hpp"
#include "types.hpp"

namespace NeoMIPS
{

	void ArgumentProcessor::SetDefaultOptions(argmap_t& map)
	{
		map.emplace(std::string("unicode"), new Option<Encoding>(Encoding::default_char));
		map.emplace(std::string("maxfreq"), new Option<uint32_t>(0xFFFFFFFF));
		map.emplace(std::string("interactive"), new Option<bool>(false));
		map.emplace(std::string("selfmodifyingcode"), new Option<bool>(false));
		map.emplace(std::string("maxmem"), new Option<uint32_t>(0xFFFFFFFF));
		map.emplace(std::string("memchunksize"), new Option<uint32_t>(0xFFFF));
		map.emplace(std::string("libs"), new Option<std::vector<std::string>>());
		map.emplace(std::string("sourcefile"), new Option<std::string>());
	}

	void ArgumentProcessor::ReadArguments(int argc, char** argv, argmap_t& argMap)
	{
		SetDefaultOptions(argMap);

		for (int i = 1; i < argc; ++i)
		{
			if (is_arg(argv[i], "-u", "--unicode"))
			{
				static_cast<Option<bool>*>(argMap.at(std::string("unicode")).get())->SetValue(true);
				continue;
			}

			if (is_arg(argv[i], "-f", "--freq"))
			{
				static_cast<Option<uint32_t>*>(argMap.at(std::string("maxfreq")).get())->SetValue(to_integer(argv[++i], IntBase::decimal));
				continue;
			}

			if (is_arg(argv[i], "-i", "--interactive"))
			{
				static_cast<Option<bool>*>(argMap.at(std::string("interactive")).get())->SetValue(true);
				continue;
			}

			if (is_arg(argv[i], "--maxmemoryusage"))
			{
				static_cast<Option<uint32_t>*>(argMap.at(std::string("maxmem")).get())->SetValue(to_integer(argv[++i], IntBase::any));
				continue;
			}

			if (is_arg(argv[i], "--memchunksize"))
			{
				static_cast<Option<uint32_t>*>(argMap.at(std::string("memchunksize")).get())->SetValue(to_integer(argv[++i], IntBase::any));
				continue;
			}

			if (is_arg(argv[i], "-s", "--selfmodifying"))
			{
				static_cast<Option<bool>*>(argMap.at(std::string("selfmodifyingcode")).get())->SetValue(true);
				continue;
			}

			static_cast<Option<std::string>*>(argMap.at(std::string("sourcefile")).get())->SetValue(std::string(argv[i]));
		}
	}
}