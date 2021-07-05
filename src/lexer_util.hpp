#pragma once
#include <string>
#include "mips32isa.hpp"
#include "types.hpp"

namespace NeoMIPS
{
	
	int index_to_line(const std::u32string& str, int index);
	bool is_space(const char32_t c);
	bool is_separator(const char32_t c);
	bool is_tag(const std::u32string& str, uint32_t index);
	std::string to_ascii_string(const std::u32string& str, bool stopAtNewline = false);
	std::u32string get_next_word(const std::u32string& str, uint32_t& offset);
	uint32_t get_reg_index(const std::u32string_view& sv);
	bool parse_instruction(const std::u32string_view& line, InstructionParameters& params, InstructionSyntacticArchetypes archetypes);
	std::optional<ISA::Instructions::Instruction> is_instruction(const std::u32string& str);
	std::optional<ISA::Directives::Directive> is_directive(const std::u32string& str);
}