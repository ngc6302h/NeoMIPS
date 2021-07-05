#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include "option.hpp"

namespace NeoMIPS
{
	using argmap_t = std::unordered_map<std::string, std::unique_ptr<OptionBase>>;

	enum class Encoding
	{
		default_char,
		utf8,
		utf16,
		utf32
	};

	enum class IntBase
	{
		decimal,
		hex,
		any
	};

	enum class InstructionSyntacticArchetypes : uint32_t
	{
		NoParams = 1 << 0,
		Imm = 1 << 1,
		Label = 1 << 2,
		Reg = 1 << 3,
		RegImm = 1 << 4,
		RegLabel = 1 << 5,
		ImmLabel = 1 << 6,
		RegReg = 1 << 7,
		RegRegReg = 1 << 8,
		RegRegImm = 1 << 9,
		RegRegLabel = 1 << 10,
		ImmRegReg = 1 << 11,
		RegMemReg = 1 << 12,
		RegOffsetForReg = 1 << 13,
		RegImmLabel = 1 << 14,
		RegLabelPlusImm = 1 << 15,
		RegLabelPlusImmOffsetForReg = 1 << 16,
		RegLabelAsOffsetReg = 1 << 17,
        CompilerGenerated = 1 << 30
	};

	inline InstructionSyntacticArchetypes operator| (InstructionSyntacticArchetypes a, InstructionSyntacticArchetypes b) { return static_cast<InstructionSyntacticArchetypes>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); }
	inline InstructionSyntacticArchetypes operator& (InstructionSyntacticArchetypes a, InstructionSyntacticArchetypes b) { return static_cast<InstructionSyntacticArchetypes>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); }
	
	struct MacroDeclaration
	{
		std::u32string m_name;
		std::vector<std::u32string> m_params;
		std::u32string m_body;
	};

	struct InstructionParameters
	{
		uint32_t m_reg1;
		uint32_t m_reg2;
		uint32_t m_reg3;
		uint32_t m_offset;
		uint32_t m_immediate;
		uint32_t m_resolvedLabel;
		std::u32string_view m_label;
		InstructionSyntacticArchetypes m_archetype;
	};
}
