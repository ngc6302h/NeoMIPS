#include <string>
#include <iostream>
#include <boost/regex.hpp>
#include <boost/regex/icu.hpp>

int main()
{
	std::u32string line = U" -12345";
	boost::match_results<std::u32string::iterator> matchResults{};
	if (boost::u32regex_search(line.begin(), line.end(), matchResults, boost::make_u32regex("((0[xX]{1}\\d+)|([-\\+]?[xX]{0}[\\dabcdefABCDEF]+))", boost::regex_constants::ECMAScript)))
	{
		std::cout << "matched somthing\n";
	}
}