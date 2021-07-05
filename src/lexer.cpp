#pragma once
#include <locale>
#include <string>
#include <algorithm>
#include <array>
#include <tuple>
#include <string>
#include <optional>
#include "include/ctre-unicode.hpp"
#include "lexer.hpp"
#include "lexer_regex.hpp"
#include "argumentprocessor.hpp"
#include "util.hpp"
#include "lexer_util.hpp"
#include "error.hpp"


namespace NeoMIPS
{
    using namespace ISA;
    using namespace ISA::Directives;
    using namespace ISA::Instructions;

    //TODO: process macros and eqv in the order they appear in the source file
    std::unique_ptr<std::vector<TokenBase*>> Lexer::Tokenize(std::u32string& source)
    {
        ResolveEQV(source);
        ResolveMacros(source);
        InitialState(source);
        return std::move(m_tokens);
    }



    void Lexer::GetMacroDeclaratios(std::u32string& source, std::vector<MacroDeclaration>& macros)
    {
        size_t i = 0;
        auto startSearchIt = source.cbegin();
        auto macroStartIt = source.cbegin();
        while ((i = source.find(U".macro", i)) != std::string::npos)
        {
            std::advance(macroStartIt, i);
            int numberOfQuotations = std::count(startSearchIt, macroStartIt, U'\"') + std::count(startSearchIt, macroStartIt, U'\'');
            if (numberOfQuotations != 0)
            {
                for (auto startItAf = startSearchIt; startItAf < macroStartIt; startItAf++)
                {
                    startItAf = std::adjacent_find(startSearchIt, macroStartIt, [](const char32_t a, const char32_t b) -> bool {return a == U'\\' && (b == U'\"' || b == U'\''); });
                    if (startItAf != macroStartIt) numberOfQuotations++;
                }
                i--;
            }
            if (numberOfQuotations % 2 == 0)
            {
                int strtindex = i;
                i += 6; //skip over .macro statement

                MacroDeclaration macro;
                std::vector<std::u32string> macroParams;
                while (is_space(source[i])) i++;
                while (!is_space(source[i])) macro.m_name += source[i++];
                //while (is_space(source[i])) i++;
                while (source[i] != U'\n')
                {
                    if (source[i] == U'%' || source[i] == U'$') //$ for compatibility with other assemblers
                    {
                        std::u32string paramName;
                        while (!is_space(source[i]))
                        {
                            if (source[i] == U',' || source[i] == U')')
                            {
                                i++;
                                break;
                            }
                            paramName += source[i++];
                        }
                        if (paramName.empty()) throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, i)), "The name of a parameter in a macro must not be empty");
                        macro.m_params.push_back(paramName);
                    }
                    else i++;
                }
                i++;
                size_t macroBodyEnd = source.find(U".end_macro", i);
                macro.m_body = source.substr(i, macroBodyEnd - i);
                macros.push_back(macro);
                source.erase(std::distance(source.cbegin(), macroStartIt), macroBodyEnd + 10 - std::distance(source.cbegin(), macroStartIt));
            }
            startSearchIt = macroStartIt;
            macroStartIt = source.cbegin();
        }
    }


    void Lexer::ResolveMacros(std::u32string& source)
    {
        std::vector<MacroDeclaration> macros;
        GetMacroDeclaratios(source, macros);
        ExpandMacros(macros, source);
    }


    //void NeoMIPS::Lexer::ExpandMacros(std::vector<NeoMIPS::MacroDeclaration>& macros, std::u32string& source)
    //{
    //    for (const MacroDeclaration& m : macros)
    //    {
    //        size_t i;
    //        while ((i = source.find(m.m_name)) != std::string::npos)
    //        {
    //            auto startIt = source.cbegin();
    //            std::advance(startIt, i);
    //            auto endIt = startIt;
    //            while (*endIt != U'\n') endIt++;
    //            //std::advance(endIt, m.m_name.size());

    //            if (m.m_params.empty())
    //            {
    //                //simply expand macro
    //                source.replace(startIt, endIt, m.m_body);
    //            }
    //            else
    //            {
    //                size_t index = i + m.m_name.size();
    //                std::u32string macroInstantiation = m.m_body;
    //                for (int c = 0; c < m.m_params.size(); c++)
    //                {
    //                    std::u32string param;
    //                    while (is_separator(source[index])) index++;
    //                    while (!is_separator(source[index])) param += source[index++];
    //                    size_t next;
    //                    while ((next = macroInstantiation.find(m.m_params[c])) != std::string::npos)
    //                    {
    //                        auto s = macroInstantiation.cbegin();
    //                        std::advance(s, next);
    //                        macroInstantiation.replace(s, s + m.m_params[c].size(), param);
    //                    }
    //                }
    //                source.replace(startIt, endIt, macroInstantiation);
    //            }
    //        }
    //    }
    //}

    //void Lexer::ResolveEQV(std::u32string& source)
    //{
    //    size_t i = 0;
    //    auto startIt = source.cbegin();
    //    auto endIt = source.cbegin();
    //    while ((i = source.find(U".eqv", i)) != std::string::npos)
    //    {
    //        endIt = source.cbegin();
    //        std::advance(endIt, i);
    //        int numberOfQuotations = std::count(startIt, endIt, U'\"') + std::count(startIt, endIt, U'\'');
    //        if (numberOfQuotations != 0)
    //        {
    //            for (auto startItAf = startIt; startItAf < endIt; startItAf++)
    //            {
    //                startItAf = std::adjacent_find(startIt, endIt, [](const char32_t a, const char32_t b) -> bool {return a == U'\\' && (b == U'\"' || b == U'\''); });
    //                if (startItAf != endIt) numberOfQuotations++;
    //            }
    //            i--;
    //        }
    //        if (numberOfQuotations % 2 == 0)
    //        {
    //            int p = i;
    //            std::u32string symbol, expression;
    //            bool expectingSomething = true;
    //            i += 4; //skip over .eqv statement
    //            while (is_space(source[i]))i++;
    //            while (!is_separator(source[i])) symbol += source[i++];
    //            while (is_space(source[i]))
    //            {
    //                if (source[i] == U'\n')
    //                {
    //                    expectingSomething = false;
    //                }
    //                i++;
    //            }
    //            if (expectingSomething) while (!is_separator(source[i])) expression += source[i++];

    //            int diff = i - p;
    //            source.erase(p, diff);
    //            i -= diff;
    //            //replace all occurences of define from here to the end of file
    //            size_t hit = 0;
    //            while ((hit = source.find(symbol, i)) != std::string::npos)
    //            {
    //                auto x = source.cbegin();
    //                auto y = source.cbegin();
    //                std::advance(x, hit);
    //                std::advance(y, hit + symbol.length());
    //                source.replace(x, y, expression);
    //                //source.erase(i, i + symbol.length());
    //                //source.insert(i, expression);
    //            }
    //        }
    //        startIt = endIt;
    //    }
    //}


    void Lexer::InitialState(const std::u32string& source)
    {
        for (m_index = 0; m_index < source.length(); ++m_index)
        {
            if (is_space(source[m_index]))continue;
            switch (source[m_index])
            {
            case U'#':
                skip_comment(source);
                break;
            case U'.':
                ParseDirective(source);
                break;
            default:
                if (isdigit(source[m_index])) //can be used because digits in utf32 are the same as in ascii
                    throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, m_index)), "Statements cannot start with a number.");
                else ParseAlphabetCharacters(source);
                break;
            }
        }
    }


    void NeoMIPS::Lexer::ParseAlphabetCharacters(const std::u32string& source)
    {
        if (is_tag(source, m_index))
        {
            ParseTag(source);
        }
        else
        {
            //std::u32string_view view(source.data() + m_index * sizeof(char32_t), source.find_first_of(U'\n', m_index));
            //
            //auto match = ctre::search<Regex::instructionPattern>(view);
            std::u32string word = get_next_word(source, m_index);
            m_index++;

            auto ins = is_instruction(word);
            if (ins)
            {
                ParseInstructionStatement(source, *ins);
            }
            else throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, m_index)), to_ascii_string(word) + " is not a valid instruction statement.");

        }
    }


    void Lexer::skip_comment(const std::u32string& source)
    {
        while (source[m_index++] != U'\n');
        m_index--;
        return;
    }


    void Lexer::ParseDirective(const std::u32string& source)
    {
        int i = m_index;
        std::u32string word = get_next_word(source, m_index);

        auto dir = is_directive(word);
        if (dir)
        {
            ParseDirectiveStatement(source, *dir);
        }
        else throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, m_index)), to_ascii_string(word) + " is not a valid directive statement.");
    }


    void Lexer::ParseTag(const std::u32string& source)
    {
        std::u32string tag;
        while (!is_separator(source[m_index]))
        {
            tag += source[m_index++];
        }
        ++m_index;
        //add token to list
    }


    void Lexer::ParseDirectiveStatement(const std::u32string& source, Directive directive)
    {
        std::vector<TokenBase*> v;
        switch (directive)
        {
        case Directive::ALIGN:
            v = DirectiveToken<Directive::ALIGN>::Parse(source, m_index);
            break;
        case Directive::ASCII:
            v = DirectiveToken<Directive::ASCII>::Parse(source, m_index);
            break;
        case Directive::ASCIIZ:
            v = DirectiveToken<Directive::ASCIIZ>::Parse(source, m_index);
            break;
        case Directive::BYTE:
            v = DirectiveToken<Directive::BYTE>::Parse(source, m_index);
            break;
        case Directive::DATA:
            v = DirectiveToken<Directive::DATA>::Parse(source, m_index);
            break;
        case Directive::DOUBLE:
            v = DirectiveToken<Directive::DOUBLE>::Parse(source, m_index);
            break;
        case Directive::FLOAT:
            v = DirectiveToken<Directive::FLOAT>::Parse(source, m_index);
            break;
        case Directive::GLOBL:
            v = DirectiveToken<Directive::GLOBL>::Parse(source, m_index);
            break;
        case Directive::HALF:
            v = DirectiveToken<Directive::HALF>::Parse(source, m_index);
            break;
        case Directive::KDATA:
            v = DirectiveToken<Directive::KDATA>::Parse(source, m_index);
            break;
        case Directive::KTEXT:
            v = DirectiveToken<Directive::KTEXT>::Parse(source, m_index);
            break;
        case Directive::SPACE:
            v = DirectiveToken<Directive::SPACE>::Parse(source, m_index);
            break;
        case Directive::TEXT:
            v = DirectiveToken<Directive::TEXT>::Parse(source, m_index);
            break;
        case Directive::WORD:
            v = DirectiveToken<Directive::WORD>::Parse(source, m_index);
            break;
        }
        m_tokens->insert(m_tokens->end(), v.begin(), v.end());
    }


    void Lexer::ParseInstructionStatement(const std::u32string& source, Instruction instruction)
    {
        size_t length = 0;
        while (source[m_index] != U'\n') length++;
        std::u32string_view instructionArgs(source.data() + m_index * sizeof(char32_t), length);
        m_index += length;

        switch (instruction)
        {
        case NeoMIPS::ISA::Instructions::Instruction::ABS_D:
            InstructionToken<Instruction::ABS_S>::Parse(instructionArgs);
            break;
        case NeoMIPS::ISA::Instructions::Instruction::ABS_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::ADD:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::ADD_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::ADD_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::ADDI:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::ADDIU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::ADDU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::AND:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::ANDI:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::BC1F:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::BC1T:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::BEQ:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::BGEZ:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::BGEZAL:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::BGTZ:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::BLEZ:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::BLTZ:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::BLTZAL:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::BNE:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::BREAK:
        case NeoMIPS::ISA::Instructions::Instruction::C_EQ_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::C_EQ_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::C_LE_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::C_LE_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::C_LT_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::C_LT_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::CEIL_W_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::CEIL_W_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::CLO:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::CLZ:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::CVT_D_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::CVT_D_W:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::CVT_S_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::CVT_S_W:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::CVT_W_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::CVT_W_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::DIV:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::DIV_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::DIV_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::DIVU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::ERET:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::FLOOR_W_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::FLOOR_W_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::J:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::JAL:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::JALR_RA:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::JALR:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::JR:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::LB:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::LBU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::LDC1:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::LH:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::LHU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::LL:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::LUI:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::LW:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::LWC1:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::LWL:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::LWR:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MADD:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MADDU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MFC0:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MFC1:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MFHI:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MFLO:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOV_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOV_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOVF:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOVF_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOVF_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOVN:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOVN_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOVN_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOVT:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOVT_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOVT_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOVZ:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOVZ_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MOVZ_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MSUB:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MSUBU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MTC0:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MTC1:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MTHI:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MTLO:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MUL:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MUL_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MUL_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MULT:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::MULTU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::NEG_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::NEG_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::NOP:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::NOR:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::OR:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::ORI:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::ROUND_W_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::ROUND_W_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SB:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SC:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SDC1:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SH:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SLL:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SLLV:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SLT:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SLTI:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SLTIU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SLTU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SQRT_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SQRT_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SRA:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SRAV:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SRL:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SRLV:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SUB:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SUB_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SUB_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SUBU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SW:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SWC1:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SWL:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SWR:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::SYSCALL:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TEQ:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TEQI:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TGE:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TGEI:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TGEIU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TGEU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TLT:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TLTI:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TLTIU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TLTU:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TNE:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TNEI:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TRUNC_W_D:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::TRUNC_W_S:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::XOR:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::XORI:
            break;
        case NeoMIPS::ISA::Instructions::Instruction::invalid:
            break;
        default:
            break;
        }
    }


    void Lexer::split_line(const std::u32string& source, std::vector<std::u32string>& strs)
    {
        while (source[m_index] != U'\n')
        {
            std::u32string str;
            while (!is_separator(source[m_index]))
            {
                str += source[m_index++];
            }
            strs.push_back(str);
        }
    }
}