#include <utility>
#include <optional>
#include "include/ctre-unicode.hpp"
#include "lexer_util.hpp"
#include "util.hpp"
#include "lexer_regex.hpp"

namespace NeoMIPS
{
	using namespace ISA;
	//Warning: this will make all information in the upper 3 bytes of each 32bit character lost
	std::string to_ascii_string(const std::u32string& str, bool stopAtNewline = false)
	{
		std::string narrowStr;
		for (char32_t c : str)
		{
			if (c == U'\n') break;
			narrowStr += static_cast<char>(c);
		}
		return narrowStr;
	}

	std::u32string get_next_word(const std::u32string& str, uint32_t& offset)
	{
		std::u32string s;
		while (!is_separator(str[offset]))
		{
			s += str[offset++];
		}
		return s;
	}

	uint32_t get_reg_index(const std::u32string_view& sv)
	{
		switch (sv[0])
		{
		case U'$':
			switch (sv[1])
			{
			case U'z':
				return 0;
			case U'a':
				return sv[2] == U't' ? 1 : 4 + sv[2] - 48;
			case U'v':
				return 2 + sv[2] - 48;
			case U't':
				return sv[2]>=8 ? 24+sv[2]-8-48 : 8 + sv[2] - 48;
			case U's':
				return sv[2] == U'p' ? 29 : 16 + sv[2] - 48;
			case U'k':
				return 26 + sv[2] - 48;
			case U'g':
				return 28;
			case U'f':
				return sv[2] == U'p' ? 30 : (sv.size() == 3 ? sv[2] - 48 : (sv[2] - 48) * 10 + sv[3] - 48);
			case U'r':
				return 31;
			}
		default:
			if (sv.size() == 2)
			{
				return sv[1] - 48;
			}
			else
			{
				return (sv[1] - 48) * 10 + sv[2] - 48;
			}
		}
	}


	bool parse_instruction(const std::u32string_view& line, InstructionParameters& params, InstructionSyntacticArchetypes archetypes)
	{
	
		if (static_cast<bool>(archetypes & InstructionSyntacticArchetypes::RegLabelAsOffsetReg))
		{
			if (auto [match, reg1, label, reg2] = ctre::search<Regex::regLabelAsOffsetRegPattern>(line); match)
			{
				
			}
		}
		if (static_cast<bool>(archetypes & InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
		{
			if (auto [match, reg1, label, immediate, reg2] = ctre::search<Regex::regLabelPlusImmOffsetForRegPattern>(line); match)
			{

			}
		}
		if (static_cast<bool>(archetypes & InstructionSyntacticArchetypes::RegLabelPlusImm))
		{
			if (auto [match, reg1, label, immediate] = ctre::search<Regex::regLabelPlusImmPattern>(line); match)
			{

			}
		}
		if (static_cast<bool>(archetypes & InstructionSyntacticArchetypes::RegOffsetForReg))
		{
			if (auto [match, reg1, offset, reg2] = ctre::search<Regex::regOffsetForRegPattern>(line); match)
			{
				
			}
		}
		if (static_cast<bool>(archetypes & InstructionSyntacticArchetypes::RegRegReg))
		{
			int count = 0;
			//std::remove_reference_t<decltype(*ctre::range<Regex::regPattern>(std::declval<std::u32string>()).begin())> reg[3];
			
			std::u32string_view reg[3];
			for (auto& match : ctre::range<Regex::regPattern>(line))
			{
				reg[count] = match;
				count++;
			}
			if (count == 3)
			{
				params.m_reg1 = get_reg_index(reg[0]);
				params.m_reg2 = get_reg_index(reg[1]);
				params.m_reg3 = get_reg_index(reg[2]);
				return true;
			}
		}
		if (static_cast<bool>(archetypes & InstructionSyntacticArchetypes::NoParams))
		{
			return true;
		}
		return false;
	}

	int index_to_line(const std::u32string& str, int index)
	{
		int counter = 0;
		while (index >= 0)
		{
			if (str[index--] == U'\n')
			{
				++counter;
			}
		}
		return counter;
	}

	bool is_space(const char32_t c)
	{
		//All characters checked against are under 0x7F so using it as an ASCII char is safe
		return std::isspace(c);
	}

	bool is_separator(const char32_t c)
	{
		switch (c)
		{
		case U' ':
		case U'\t':
		case U'\n':
		case U':':
		case U'(':
		case U')':
		case U'"':
		case U',':
		case U'+':
		case U'-':
			return true;
		default:
			return false;
		}
	}

	std::optional<Instruction> is_instruction(const std::u32string& str)
	{
		auto hit = std::find_if(INSTRUCTIONS.cbegin(), INSTRUCTIONS.cend(), [&](const std::pair<const std::u32string_view, Instruction>& p) constexpr {return p.first == str; });
		return hit != INSTRUCTIONS.cend() ? std::make_optional((*hit).second) : std::nullopt;
	}

	std::optional<Directive> is_directive(const std::u32string& str)
	{
		auto hit = std::find_if(ISA::DIRECTIVES.cbegin(), DIRECTIVES.cend(), [&](const std::pair<const std::u32string_view, Directive>& p) constexpr {return p.first == str; });
		return hit != DIRECTIVES.cend() ? std::make_optional((*hit).second) : std::nullopt;
	}

	bool is_tag(const std::u32string& str, uint32_t index)
	{
		while (is_separator(str[index]))index++;
		for (; !is_separator(str[index]); ++index)
		{
			if (str[index + 1] == U':') return true;
		}
		return false;
	}
}
