#pragma once
#include <vector>
#include <string>
#include "mips32isa.hpp"
#include "util.hpp"
#include "lexer_util.hpp"
#include "error.hpp"

namespace NeoMIPS
{
    using namespace ISA::Directives;
    using namespace ISA::Instructions;
    using namespace ISA::PseudoInstructions;
    using namespace ISA::Encoding;


    enum class TokenType
    {
        Directive,
        Instruction,
        Pseudoinstruction,
        Tag
    };

    class TokenBase
    {
    public:
        virtual TokenType GetTokenType() = 0;
    };

    class InstructionTokenBase : public TokenBase
    {
    public:
        InstructionTokenBase() : m_parameters() {};
        InstructionParameters m_parameters;
        virtual TokenType GetTokenType() { return TokenType::Instruction; }
        virtual uint32_t Encode() = 0;
        virtual void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos)
        {
            return;
        }
    };

    class PseudoinstructionTokenBase : public TokenBase
    {
    public:
        PseudoinstructionTokenBase() : m_parameters() {};
        InstructionParameters m_parameters;
        virtual TokenType GetTokenType() { return TokenType::Pseudoinstruction; }
    };

    template<ISA::Instruction I>
    class InstructionToken : public InstructionTokenBase
    {
    };

    template<ISA::Pseudoinstruction I>
    class PseudoinstructionToken : public PseudoinstructionTokenBase
    {
    };


    class DirectiveTokenBase : public TokenBase
    {
    public:
        virtual Directive GetDirective() = 0;
        virtual TokenType GetTokenType() { return TokenType::Directive; }

    };

    template<ISA::Directive D>
    class DirectiveToken : public DirectiveTokenBase
    {
        virtual Directive GetDirective() override { return Directive::invalid; }
    };

    //Specializations for directives

    template<>
    class DirectiveToken<ISA::Directive::ALIGN> : public DirectiveTokenBase
    {
        uint32_t m_alignment;
        DirectiveToken(uint32_t alignment) : m_alignment(alignment) {}
    public:
        virtual Directive GetDirective() override { return Directive::ALIGN; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;
            while (!isdigit(source[index])) //skip whitespace
            {
                if (source[index++] == U'\n')
                {
                    throw Error::InvalidDirectiveException(std::to_string(index_to_line(source, index)), "ALIGN directive must be followed by an alignment value.");
                }
            }

            auto alignment = to_integer(to_ascii_string(get_next_word(source, index)).c_str());

            if (alignment >= 0 && alignment < 4)
            {
                tokens.emplace_back(new DirectiveToken(alignment));
            }
            else throw Error::InvalidDirectiveException(std::to_string(index_to_line(source, index)), "Alignment can only be 0(0 bytes), 1(2 bytes), 2(4bytes) or 3(8 bytes).");
            return tokens;
        }
    };

    template<>
    class DirectiveToken<ISA::Directive::ASCII> : public DirectiveTokenBase
    {
        std::u32string m_string;
        DirectiveToken(const std::u32string& string) : m_string(string)
        {
            if (string.empty())
            {
                throw Error::InvalidDirectiveException("", "Can't have an empty string.");
            }
        }
        static void ParseStringLiteral(const std::u32string& source, uint32_t& index, std::vector<NeoMIPS::TokenBase*>& tokens)
        {
            while (source[index] != U'"' && source[index] != '\'') //skip whitespace
            {
                if (source[index++] == U'\n')
                {
                    throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "ASCII directive must be followed by a string literal in quotations.");
                }
            }
            std::u32string str;
            for (index++; source[index] != U'\'' && source[index] != U'\"';)
            {
                if (source[index] == U'\n')
                {
                    throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "ASCII directive must be followed by a string literal in quotations.");
                }

                if (source[index] != U'\\')
                {
                    str += source[index];
                }
                else //parse escape sequence
                {
                    switch (source[++index])
                    {
                    case U'n':
                        str += '\n';
                        break;
                    case U'r':
                        str += '\r';
                        break;
                    case U't':
                        str += '\t';
                        break;
                    case U'\\':
                        str += '\\';
                        break;
                    case U'\"':
                        str += '\"';
                        break;
                    case U'\'':
                        str += '\"';
                        break;
                    case U'x':
                        str += static_cast<char32_t>(to_integer(to_ascii_string(source.substr(index, 2)).c_str(), NeoMIPS::IntBase::hex));
                        index += 2;
                        break;
                    case U'u':
                        str += static_cast<char32_t>(to_integer(to_ascii_string(source.substr(index, 4)).c_str(), NeoMIPS::IntBase::hex));
                        index += 4;
                        break;
                    case U'U':
                        str += static_cast<char32_t>(to_integer(to_ascii_string(source.substr(index, 8)).c_str(), NeoMIPS::IntBase::hex));
                        index += 8;
                        break;
                    default:
                        throw Error::InvalidEscapeSequenceException(std::to_string(index_to_line(source, index)), std::string("Error while parsing ASCII directive: \"").append(to_ascii_string(source.substr(index, 2))).append("\" is not a valid escape sequence."));
                    }
                }
                index++;
            }
            index++;
            tokens.emplace_back(new DirectiveToken(str));
        }
    public:
        virtual Directive GetDirective() override { return Directive::ASCII; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;
            bool anotherString = false;
            do
            {
                ParseStringLiteral(source, index, tokens);
                while (source[index] != U'\n')
                {
                    if (source[index++] == U',')
                    {
                        anotherString = true;
                        break;
                    }
                }
            } while (anotherString);

            return tokens;
        }

    };

    template<>
    class DirectiveToken<ISA::Directive::ASCIIZ> : public DirectiveTokenBase
    {
        std::u32string m_string;
        DirectiveToken(const std::u32string& string) : m_string(string)
        {
            if (string.empty())
            {
                throw Error::InvalidDirectiveException("unknown", "Can't have an empty string.");
            }
        }
        static void ParseStringLiteral(const std::u32string& source, uint32_t& index, std::vector<NeoMIPS::TokenBase*>& tokens)
        {
            while (source[index] != U'"' && source[index] != '\'') //skip whitespace
            {
                if (source[index++] == U'\n')
                {
                    throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "ASCIIZ directive must be followed by a string literal in quotations.");
                }
            }
            std::u32string str;
            for (index++; source[index] != U'\'' && source[index] != U'\"';)
            {
                if (source[index] == U'\n')
                {
                    throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "ASCIIZ directive must be followed by a string literal in quotations.");
                }

                if (source[index] != U'\\')
                {
                    str += source[index];
                }
                else //parse escape sequence
                {
                    switch (source[++index])
                    {
                    case U'n':
                        str += '\n';
                        break;
                    case U'r':
                        str += '\r';
                        break;
                    case U't':
                        str += '\t';
                        break;
                    case U'\\':
                        str += '\\';
                        break;
                    case U'\"':
                        str += '\"';
                        break;
                    case U'\'':
                        str += '\"';
                        break;
                    case U'x':
                        str += static_cast<char32_t>(to_integer(to_ascii_string(source.substr(index, 2)).c_str(), NeoMIPS::IntBase::hex));
                        break;
                    case U'u':
                        str += static_cast<char32_t>(to_integer(to_ascii_string(source.substr(index, 4)).c_str(), NeoMIPS::IntBase::hex));
                        break;
                    case U'U':
                        str += static_cast<char32_t>(to_integer(to_ascii_string(source.substr(index, 8)).c_str(), NeoMIPS::IntBase::hex));
                        break;
                    default:
                        throw Error::InvalidEscapeSequenceException(std::to_string(index_to_line(source, index)), std::string("Error while parsing ASCIIZ directive: \"").append(to_ascii_string(source.substr(index, 2))).append("\" is not a valid escape sequence."));
                    }
                }
                index++;
            }
            index++;
            str += U'\0';
            tokens.emplace_back(reinterpret_cast<TokenBase*>(new DirectiveToken(str)));
        }
    public:
        virtual Directive GetDirective() override { return Directive::ASCIIZ; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;
            bool anotherString = false;
            do
            {
                ParseStringLiteral(source, index, tokens);
                while (source[index] != U'\n')
                {
                    if (source[index++] == U',')
                    {
                        anotherString = true;
                        break;
                    }
                }
            } while (anotherString);

            return tokens;
        }
    };

    template<>
    class DirectiveToken<ISA::Directive::BYTE> : public DirectiveTokenBase
    {
        uint8_t m_byte;
        DirectiveToken(uint8_t byte) : m_byte(byte) {}
    public:
        virtual Directive GetDirective() override { return Directive::BYTE; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;

            bool anotherByte = false;
            do
            {
                while (is_separator(source[index]))
                {
                    if (source[index++] == U'\n')
                    {
                        throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "BYTE directive must be followed by a valid integer literal.");
                    }
                }
                if (source[index - 1] == U'+' || source[index - 1] == U'-') index--;
                std::u32string str;
                while (!is_separator(source[index]))
                {
                    str += source[index++];
                }
                try
                {
                    tokens.emplace_back(new DirectiveToken(static_cast<uint8_t>(to_integer(to_ascii_string(str).c_str()))));
                }
                catch (std::exception& e)
                {
                    throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "Error parsing integer literal.");
                }
                while (source[index] != U'\n')
                {
                    if (source[index++] == U',')
                    {
                        anotherByte = true;
                        break;
                    }
                }
            } while (anotherByte);
            return tokens;
        }
    };

    template<>
    class DirectiveToken<ISA::Directive::DATA> : public DirectiveTokenBase
    {
        uint32_t m_startAddr;
        DirectiveToken(uint32_t startAddr) : m_startAddr(startAddr) {}
    public:
        virtual Directive GetDirective() override { return Directive::DATA; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;
            while (is_separator(source[index]))
            {
                if (source[index++] == U'\n')
                {
                    tokens.emplace_back(new DirectiveToken(0x10000000));
                    return tokens;
                }
            }
            std::u32string str;
            while (!is_separator(source[index]))
            {
                str += source[index++];
            }
            try
            {
                tokens.emplace_back(new DirectiveToken(to_integer(to_ascii_string(str).c_str())));
            }
            catch (std::exception& e)
            {
                throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "Error parsing integer literal.");
            }
            return tokens;
        }
    };


    template<>
    class DirectiveToken<Directive::DOUBLE> : public DirectiveTokenBase
    {
        double m_double;
        DirectiveToken(double d) : m_double(d) {}
    public:
        virtual Directive GetDirective() override { return Directive::DOUBLE; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;
            bool anotherDouble = false;
            do
            {
                while (is_separator(source[index]))
                {
                    if (source[index++] == U'\n')
                    {
                        throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "DOUBLE directive must be followed by a valid double literal.");
                    }
                }
                if (source[index - 1] == U'+' || source[index - 1] == U'-') index--;
                std::u32string str;
                while (!is_separator(source[index]))
                {
                    str += source[index++];
                }
                try
                {
                    tokens.emplace_back(new DirectiveToken((to_double(to_ascii_string(str)))));
                }
                catch (std::exception& e)
                {
                    throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "Error parsing double literal.");
                }
                while (source[index] != U'\n')
                {
                    if (source[index++] == U',')
                    {
                        anotherDouble = true;
                        break;
                    }
                }
            } while (anotherDouble);
            return tokens;
        }
    };


    template<>
    class DirectiveToken<Directive::FLOAT> : public DirectiveTokenBase
    {
        float m_float;
        DirectiveToken(float f) : m_float(f) {}
    public:
        virtual Directive GetDirective() override { return Directive::FLOAT; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;
            bool anotherFloat = false;
            do
            {
                while (is_separator(source[index]))
                {
                    if (source[index++] == U'\n')
                    {
                        throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "FLOAT directive must be followed by a valid float literal.");
                    }
                }
                if (source[index - 1] == U'+' || source[index - 1] == U'-') index--;
                std::u32string str;
                while (!is_separator(source[index]))
                {
                    str += source[index++];
                }
                try
                {
                    tokens.emplace_back(new DirectiveToken((to_float(to_ascii_string(str)))));
                }
                catch (std::exception& e)
                {
                    throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "Error parsing float literal.");
                }
                while (source[index] != U'\n')
                {
                    if (source[index++] == U',')
                    {
                        anotherFloat = true;
                        break;
                    }
                }
            } while (anotherFloat);
            return tokens;
        }
    };

    template<>
    class DirectiveToken<ISA::Directive::GLOBL> : public DirectiveTokenBase
    {
        std::u32string m_globalSymbol;
        DirectiveToken(const std::u32string& symbol) : m_globalSymbol(symbol) {}
    public:
        virtual Directive GetDirective() override { return Directive::GLOBL; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;
            std::u32string str;
            while (is_space(source[index])) index++;
            while (!is_separator(source[index]))
            {
                str += source[index++];
            }
            tokens.emplace_back(new DirectiveToken(str));
            return tokens;
        }
    };

    template<>
    class DirectiveToken<ISA::Directive::HALF> : public DirectiveTokenBase
    {
        uint16_t m_half;
        DirectiveToken(uint16_t half) : m_half(half) {}
    public:
        virtual Directive GetDirective() override { return Directive::HALF; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;

            bool anotherHalf = false;
            do
            {
                while (is_separator(source[index]))
                {
                    if (source[index++] == U'\n')
                    {
                        throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "HALF directive must be followed by a valid integer literal.");
                    }
                }
                if (source[index - 1] == U'+' || source[index - 1] == U'-') index--;
                std::u32string str;
                while (!is_separator(source[index]))
                {
                    str += source[index++];
                }
                try
                {
                    tokens.emplace_back(new DirectiveToken(to_float(to_ascii_string(str))));
                }
                catch (std::exception& e)
                {
                    throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "Error parsing half literal.");
                }
                while (source[index] != U'\n')
                {
                    if (source[index++] == U',')
                    {
                        anotherHalf = true;
                        break;
                    }
                }
            } while (anotherHalf);
            return tokens;
        }
    };

    template<>
    class DirectiveToken<ISA::Directive::KDATA> : public DirectiveTokenBase
    {
        uint32_t m_startAddr;
        DirectiveToken(uint32_t startAddr) : m_startAddr(startAddr) {}
    public:
        virtual Directive GetDirective() override { return Directive::KDATA; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;
            while (is_separator(source[index]))
            {
                if (source[index++] == U'\n')
                {
                    tokens.emplace_back(new DirectiveToken(0x90000000));
                    return tokens;
                }
            }
            std::u32string str;
            while (!is_separator(source[index]))
            {
                str += source[index++];
            }
            try
            {
                tokens.emplace_back(new DirectiveToken(to_integer(to_ascii_string(str).c_str())));
            }
            catch (std::exception& e)
            {
                throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "Error parsing integer literal.");
            }
            return tokens;
        }
    };

    template<>
    class DirectiveToken<ISA::Directive::KTEXT> : public DirectiveTokenBase
    {
        uint32_t m_startAddr;
        DirectiveToken(uint32_t startAddr) : m_startAddr(startAddr) {}
    public:
        virtual Directive GetDirective() override { return Directive::KTEXT; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;
            while (is_separator(source[index]))
            {
                if (source[index++] == U'\n')
                {
                    tokens.emplace_back(new DirectiveToken(0x80000000));
                    return tokens;
                }
            }
            std::u32string str;
            while (!is_separator(source[index]))
            {
                str += source[index++];
            }
            try
            {
                tokens.emplace_back(new DirectiveToken(to_integer(to_ascii_string(str).c_str())));
            }
            catch (std::exception& e)
            {
                throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "Error parsing integer literal.");
            }
            return tokens;
        }
    };

    template<>
    class DirectiveToken<ISA::Directive::SPACE> : public DirectiveTokenBase
    {
        uint16_t m_space;
        DirectiveToken(uint32_t space) : m_space(space) {}
    public:
        virtual Directive GetDirective() override { return Directive::SPACE; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;

            bool anotherSpace = false;
            do
            {
                while (is_separator(source[index]))
                {
                    if (source[index++] == U'\n')
                    {
                        throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "SPACE directive must be followed by a valid integer literal.");
                    }
                }
                if (source[index - 1] == U'+' || source[index - 1] == U'-') index--;
                std::u32string str;
                while (!is_separator(source[index]))
                {
                    str += source[index++];
                }
                try
                {
                    tokens.emplace_back(new DirectiveToken(static_cast<uint32_t>(to_integer(to_ascii_string(str).c_str()))));
                }
                catch (std::exception& e)
                {
                    throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "Error parsing integer literal.");
                }
                while (source[index] != U'\n')
                {
                    if (source[index++] == U',')
                    {
                        anotherSpace = true;
                        break;
                    }
                }
            } while (anotherSpace);
            return tokens;
        }
    };

    template<>
    class DirectiveToken<ISA::Directive::WORD> : public DirectiveTokenBase
    {
        uint16_t m_word;
        DirectiveToken(uint32_t word) : m_word(word) {}
    public:
        virtual Directive GetDirective() override { return Directive::WORD; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;

            bool anotherWord = false;
            do
            {
                while (is_separator(source[index]))
                {
                    if (source[index++] == U'\n')
                    {
                        throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "WORD directive must be followed by a valid integer literal.");
                    }
                }
                if (source[index - 1] == U'+' || source[index - 1] == U'-') index--;
                std::u32string str;
                while (!is_separator(source[index]))
                {
                    str += source[index++];
                }
                try
                {
                    tokens.emplace_back(new DirectiveToken(static_cast<uint32_t>(to_integer(to_ascii_string(str).c_str()))));
                }
                catch (std::exception& e)
                {
                    throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "Error parsing integer literal.");
                }
                while (source[index] != U'\n')
                {
                    if (source[index++] == U',')
                    {
                        anotherWord = true;
                        break;
                    }
                }
            } while (anotherWord);
            return tokens;
        }
    };

    template<>
    class DirectiveToken<ISA::Directive::TEXT> : public DirectiveTokenBase
    {
        uint32_t m_startAddr;
        DirectiveToken(uint32_t startAddr) : m_startAddr(startAddr) {}
    public:
        virtual Directive GetDirective() override { return Directive::TEXT; }
        static std::vector<TokenBase*> Parse(const std::u32string& source, uint32_t& index)
        {
            std::vector<TokenBase*> tokens;
            while (is_separator(source[index]))
            {
                if (source[index++] == U'\n')
                {
                    tokens.emplace_back(new DirectiveToken(0x04000000));
                    return tokens;
                }
            }
            std::u32string str;
            while (!is_separator(source[index]))
            {
                str += source[index++];
            }
            try
            {
                tokens.emplace_back(new DirectiveToken(to_integer(to_ascii_string(str).c_str())));
            }
            catch (std::exception& e)
            {
                throw Error::InvalidSyntaxException(std::to_string(index_to_line(source, index)), "Error parsing integer literal.");
            }
            return tokens;
        }
    };

    //Specializations for instructions

    template<>
    class InstructionToken<Instruction::ABS_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the abs.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0 || params.m_reg2 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                else
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000101;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::ABS_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the abs.s instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0 || params.m_reg2 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                else
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000101;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::ADD> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the add instruction");
            }
            else
            {
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (keepPseudoinstructions)
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    else
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::ADD>*>(vec.emplace_back(new InstructionToken<Instruction::ADD>()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = params.m_reg2;
                            c->m_parameters.m_reg3 = 1;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                }

                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000;
            instruction |= 0b100000;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::ADD_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            auto i = static_cast<InstructionToken<Instruction::ADD_D>*>(vec.emplace_back(new InstructionToken<Instruction::ADD_D>()));
            if (!parse_instruction(instructionStr, i->m_parameters, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the add.d instruction");
            }
            if (i->m_parameters.m_reg1 % 2 != 0 || i->m_parameters.m_reg2 % 2 != 0 || i->m_parameters.m_reg3 % 2 != 0)
            {
                throw  Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000000;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::ADD_S> : public InstructionTokenBase
    {
        InstructionToken() : InstructionTokenBase() {}
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            auto i = static_cast<InstructionToken<Instruction::ADD_S>*>(vec.emplace_back(new InstructionToken<Instruction::ADD_S>()));
            if (!parse_instruction(instructionStr, i->m_parameters, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the add.s instruction");
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000000;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::ADDI> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the addi instruction");
            }
            else
            {
                if (std::in_range<int16_t>(params.m_immediate))
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else if (!keepPseudoinstructions)
                {
                    auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                    a->m_parameters.m_reg1 = 1;
                    a->m_parameters.m_immediate = params.m_immediate >> 16;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                    b->m_parameters.m_reg1 = 1;
                    b->m_parameters.m_reg2 = 1;
                    b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto c = static_cast<InstructionToken<Instruction::ADD>*>(vec.emplace_back(new InstructionToken<Instruction::ADD>()));
                    c->m_parameters.m_reg1 = params.m_reg1;
                    c->m_parameters.m_reg2 = params.m_reg2;
                    c->m_parameters.m_reg3 = 1;
                    c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;
                }
                else
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                }

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b001000 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= m_parameters.m_immediate;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::ADDIU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the addiu instruction");
            }
            else if (!keepPseudoinstructions)
            {
                auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                a->m_parameters.m_reg1 = 1;
                a->m_parameters.m_immediate = params.m_immediate >> 16;
                a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                b->m_parameters.m_reg1 = 1;
                b->m_parameters.m_reg2 = 1;
                b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                auto c = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                c->m_parameters.m_reg1 = params.m_reg1;
                c->m_parameters.m_reg2 = params.m_reg2;
                c->m_parameters.m_reg3 = 1;
                c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                return vec;
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b001001 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= m_parameters.m_immediate;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::ADDU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the addu instruction");
            }
            else
            {
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (keepPseudoinstructions)
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::ADD>*>(vec.emplace_back(new InstructionToken<Instruction::ADD>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg2;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;

                    }
                }

                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000;
            instruction |= 0b100001;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::AND> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm | InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the and instruction");
            }
            else
            {
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegImm:
                    params.m_reg2 = params.m_reg1;
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (keepPseudoinstructions)
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    else
                    {
                        static_cast<InstructionToken<Instruction::ANDI>*>(vec.emplace_back(new InstructionToken<Instruction::ANDI>()))->m_parameters = params;
                        return vec;
                    }
                }
                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000;
            instruction |= 0b100100;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::ANDI> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegImm | InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the and instruction");
            }
            else
            {
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegImm:
                    params.m_reg2 = params.m_reg1;
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (std::in_range<uint16_t>(params.m_immediate))
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    else
                    {
                        if (keepPseudoinstructions)
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                            auto c = static_cast<InstructionToken<Instruction::ADD>*>(vec.emplace_back(new InstructionToken<Instruction::AND>()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = params.m_reg2;
                            c->m_parameters.m_reg3 = 1;
                            return vec;
                        }
                    }
                }
                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b001100 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= m_parameters.m_immediate;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::B> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::Label))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the b instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    auto a = static_cast<InstructionToken<Instruction::BGEZ>*>(vec.emplace_back(new InstructionToken<Instruction::BGEZ>()));
                    a->m_parameters.m_reg1 = 0;
                    a->m_parameters.m_label = params.m_label;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::BC1F> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::Label | InstructionSyntacticArchetypes::ImmLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the bc1f instruction");
            }
            else
            {
                if (params.m_archetype == InstructionSyntacticArchetypes::Label)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                }
                else if (params.m_immediate > 7 || params.m_immediate < 0)
                {
                    throw Error::InvalidSyntaxException("?", "Flag for instruction bc1f must be in the [0-7] range");
                }
                else
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                }
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= 0b01000 << 21;
            instruction |= m_parameters.m_immediate << 18;
            instruction |= 0b0 << 17;
            instruction |= 0b0 << 16;
            instruction |= m_parameters.m_resolvedLabel & 0xFFFF; //here, m_resolvedLabel must be ((labelAddress - currentAddress) >> 2). IMPORTANT!
        }
    };

    template<>
    class InstructionToken<Instruction::BC1T> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::Label | InstructionSyntacticArchetypes::ImmLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the bc1t instruction");
            }
            else
            {
                if (params.m_archetype == InstructionSyntacticArchetypes::Label)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                }
                else if (params.m_immediate > 7 || params.m_immediate < 0)
                {
                    throw Error::InvalidSyntaxException("?", "Flag for instruction bc1t must be in the [0-7] range");
                }
                else
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                }
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= 0b01000 << 21;
            instruction |= m_parameters.m_immediate << 18;
            instruction |= 0b0 << 17;
            instruction |= 0b1 << 16;
            instruction |= m_parameters.m_resolvedLabel & 0xFFFF; //here, m_resolvedLabel must be ((labelAddress - currentAddress) >> 2). IMPORTANT!
        }
    };

    template<>
    class InstructionToken<Instruction::BEQ> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegLabel | InstructionSyntacticArchetypes::RegImmLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the beq instruction");
            }
            else
            {
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegLabel:
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (keepPseudoinstructions)
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    else
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_immediate = params.m_immediate;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = params.m_reg1;
                            b->m_parameters.m_label = params.m_label;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                            c->m_parameters.m_reg1 = 1;
                            c->m_parameters.m_reg2 = params.m_reg1;
                            c->m_parameters.m_label = params.m_label;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                }

                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b000100 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_resolvedLabel & 0xFFFF; //here, m_resolvedLabel must be ((labelAddress - currentAddress) >> 2). IMPORTANT!
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::BEQZ> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the beqz instruction");
            }
            else if (!keepPseudoinstructions)
            {
                auto a = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                a->m_parameters.m_reg1 = params.m_reg1;
                a->m_parameters.m_reg2 = 0;
                a->m_parameters.m_label = params.m_label;
                a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                return vec;
            }
            else
            {
                auto a = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                a->m_parameters.m_reg1 = params.m_reg1;
                a->m_parameters.m_reg2 = 0;
                a->m_parameters.m_label = params.m_label;
                a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                return vec;
            }
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::BGE> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegLabel | InstructionSyntacticArchetypes::RegImmLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the bge instruction");
            }
            else
            {
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegLabel:
                {
                    if (keepPseudoinstructions)
                    {
                        static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = params.m_reg1;
                        a->m_parameters.m_reg3 = params.m_reg2;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 0;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                }
                case InstructionSyntacticArchetypes::RegImmLabel:
                {
                    if (keepPseudoinstructions)
                    {
                        static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    else
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken<Instruction::SLTI>*>(vec.emplace_back(new InstructionToken<Instruction::SLTI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_reg2 = params.m_reg1;
                            a->m_parameters.m_immediate = params.m_immediate;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 0;
                            b->m_parameters.m_label = params.m_label;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                            c->m_parameters.m_reg1 = 1;
                            c->m_parameters.m_reg2 = params.m_reg1;
                            c->m_parameters.m_reg3 = 1;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto d = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                            d->m_parameters.m_reg1 = 1;
                            d->m_parameters.m_reg2 = 0;
                            d->m_parameters.m_label = params.m_label;
                            d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::BGEU> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegLabel | InstructionSyntacticArchetypes::RegImmLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the bgeu instruction");
            }
            else
            {
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegLabel:
                {
                    if (keepPseudoinstructions)
                    {
                        static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    else
                    {

                        auto a = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = params.m_reg1;
                        a->m_parameters.m_reg3 = params.m_reg2;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 0;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                }
                case InstructionSyntacticArchetypes::RegImmLabel:
                {
                    if (keepPseudoinstructions)
                    {
                        static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    else
                    {
                        if (std::in_range<uint16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken<Instruction::SLTIU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTIU>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_reg2 = params.m_reg1;
                            a->m_parameters.m_immediate = params.m_immediate;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 0;
                            b->m_parameters.m_label = params.m_label;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                            c->m_parameters.m_reg1 = 1;
                            c->m_parameters.m_reg2 = params.m_reg1;
                            c->m_parameters.m_reg3 = 1;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto d = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                            d->m_parameters.m_reg1 = 1;
                            d->m_parameters.m_reg2 = 0;
                            d->m_parameters.m_label = params.m_label;
                            d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                }
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::BGEZ> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the bgez instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b000001 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= 0b00001 << 16;
            instruction |= m_parameters.m_resolvedLabel & 0xFFFF; //here, m_resolvedLabel must be ((labelAddress - currentAddress) >> 2). IMPORTANT!
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::BGEZAL> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the bgezal instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b000001 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= 0b10001 << 16;
            instruction |= m_parameters.m_resolvedLabel & 0xFFFF; //here, m_resolvedLabel must be ((labelAddress - currentAddress) >> 2). IMPORTANT!
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::BGT> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegLabel | InstructionSyntacticArchetypes::RegImmLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the bgtu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegLabel:
                {
                    auto a = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                    a->m_parameters.m_reg1 = 1;
                    a->m_parameters.m_reg2 = params.m_reg2;
                    a->m_parameters.m_reg3 = params.m_reg1;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                    b->m_parameters.m_reg1 = 1;
                    b->m_parameters.m_reg2 = 0;
                    b->m_parameters.m_label = params.m_label;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;

                }
                case InstructionSyntacticArchetypes::RegImmLabel:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = 0;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = params.m_reg1;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = 0;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::BGTU> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegLabel | InstructionSyntacticArchetypes::RegImmLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the bgtu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegLabel:
                {

                    auto a = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                    a->m_parameters.m_reg1 = 1;
                    a->m_parameters.m_reg2 = params.m_reg2;
                    a->m_parameters.m_reg3 = params.m_reg1;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                    b->m_parameters.m_reg1 = 1;
                    b->m_parameters.m_reg2 = 0;
                    b->m_parameters.m_label = params.m_label;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;

                }
                case InstructionSyntacticArchetypes::RegImmLabel:
                {
                    if (std::in_range<uint16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = 0;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_reg3 = params.m_reg1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = 0;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::BGTZ> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the bgtz instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b000111 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_resolvedLabel & 0xFFFF; //here, m_resolvedLabel must be ((labelAddress - currentAddress) >> 2). IMPORTANT!
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::BLE> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegLabel | InstructionSyntacticArchetypes::RegImmLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the ble instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegLabel:
                {

                    auto a = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                    a->m_parameters.m_reg1 = 1;
                    a->m_parameters.m_reg2 = params.m_reg2;
                    a->m_parameters.m_reg3 = params.m_reg1;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                    b->m_parameters.m_reg1 = 1;
                    b->m_parameters.m_reg2 = 0;
                    b->m_parameters.m_label = params.m_label;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;

                }
                case InstructionSyntacticArchetypes::RegImmLabel:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = params.m_reg1;
                        a->m_parameters.m_immediate = -1;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SLTI>*>(vec.emplace_back(new InstructionToken<Instruction::SLTI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = 0;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = params.m_reg1;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = 0;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::BLEU> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegLabel | InstructionSyntacticArchetypes::RegImmLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the bleu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegLabel:
                {
                    auto a = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                    a->m_parameters.m_reg1 = 1;
                    a->m_parameters.m_reg2 = params.m_reg2;
                    a->m_parameters.m_reg3 = params.m_reg1;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                    b->m_parameters.m_reg1 = 1;
                    b->m_parameters.m_reg2 = 0;
                    b->m_parameters.m_label = params.m_label;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;

                }
                case InstructionSyntacticArchetypes::RegImmLabel:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = params.m_reg1;
                        a->m_parameters.m_immediate = -1;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SLTI>*>(vec.emplace_back(new InstructionToken<Instruction::SLTI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = 0;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_reg3 = params.m_reg1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = 0;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::BLT> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegLabel | InstructionSyntacticArchetypes::RegImmLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the blt instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegLabel:
                {
                    auto a = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                    a->m_parameters.m_reg1 = 1;
                    a->m_parameters.m_reg2 = params.m_reg1;
                    a->m_parameters.m_reg3 = params.m_reg2;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                    b->m_parameters.m_reg1 = 1;
                    b->m_parameters.m_reg2 = 0;
                    b->m_parameters.m_label = params.m_label;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;

                }
                case InstructionSyntacticArchetypes::RegImmLabel:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::SLTI>*>(vec.emplace_back(new InstructionToken<Instruction::SLTI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = params.m_reg1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 0;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = params.m_reg1;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = 0;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::BLTU> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegLabel | InstructionSyntacticArchetypes::RegImmLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the bltu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegLabel:
                {
                    auto a = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                    a->m_parameters.m_reg1 = 1;
                    a->m_parameters.m_reg2 = params.m_reg1;
                    a->m_parameters.m_reg3 = params.m_reg2;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                    b->m_parameters.m_reg1 = 1;
                    b->m_parameters.m_reg2 = 0;
                    b->m_parameters.m_label = params.m_label;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;

                }
                case InstructionSyntacticArchetypes::RegImmLabel:
                {

                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::SLTIU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTIU>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = params.m_reg1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 0;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = params.m_reg1;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = 0;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::BNE> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegLabel | InstructionSyntacticArchetypes::RegImmLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the bne instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegLabel:
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = params.m_reg1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = params.m_reg1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }

                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b000101 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_resolvedLabel & 0xFFFF; //here, m_resolvedLabel must be ((labelAddress - currentAddress) >> 2). IMPORTANT!
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::BNEZ> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegLabel))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the bnez instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    auto a = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                    a->m_parameters.m_reg1 = params.m_reg1;
                    a->m_parameters.m_reg2 = 0;
                    a->m_parameters.m_label = params.m_label;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                }

            }
        }
    };

    template<>
    class InstructionToken<Instruction::BREAK> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::NoParams | InstructionSyntacticArchetypes::Imm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the break instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_immediate << 6;
            instruction |= 0b001101;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::C_EQ_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::ImmRegReg | InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the c.eq.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0 || params.m_reg2 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params; //zero initialization takes care of setting m_immediate to 0 in case user does not provide it
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= m_parameters.m_immediate & 0b111 << 8;
            instruction |= 0b00 << 6;
            instruction |= 0b11 << 4;
            instruction |= 0b0010;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::C_EQ_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::ImmRegReg | InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the c.eq.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params; //zero initialization takes care of setting m_immediate to 0 in case user does not provide it
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= m_parameters.m_immediate & 0b111 << 8;
            instruction |= 0b00 << 6;
            instruction |= 0b11 << 4;
            instruction |= 0b0010;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::C_LE_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::ImmRegReg | InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the c.le.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0 || params.m_reg2 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params; //zero initialization takes care of setting m_immediate to 0 in case user does not provide it
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= m_parameters.m_immediate & 0b111 << 8;
            instruction |= 0b00 << 6;
            instruction |= 0b11 << 4;
            instruction |= 0b1110;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::C_LE_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::ImmRegReg | InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the c.le.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params; //zero initialization takes care of setting m_immediate to 0 in case user does not provide it
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= m_parameters.m_immediate & 0b111 << 8;
            instruction |= 0b00 << 6;
            instruction |= 0b11 << 4;
            instruction |= 0b1110;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::C_LT_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::ImmRegReg | InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the c.lt.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0 || params.m_reg2 % 2 != 0)
                {
                    throw  Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params; //zero initialization takes care of setting m_immediate to 0 in case user does not provide it
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= m_parameters.m_immediate & 0b111 << 8;
            instruction |= 0b00 << 6;
            instruction |= 0b11 << 4;
            instruction |= 0b0100;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::C_LT_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::ImmRegReg | InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the c.lt.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params; //zero initialization takes care of setting m_immediate to 0 in case user does not provide it
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= m_parameters.m_immediate & 0b111 << 8;
            instruction |= 0b00 << 6;
            instruction |= 0b11 << 4;
            instruction |= 0b0100;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::CEIL_W_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the ceil.w.d instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b001110;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::CEIL_W_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the ceil.w.s instruction");
            }
            else
            {

                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b001110;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::CLO> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the clo instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b011100 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg2 << 16; //yes, this is correct
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000 << 6;
            instruction |= 0b100001;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::CLZ> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the clz instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b011100 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg2 << 16; //yes, this is correct
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000 << 6;
            instruction |= 0b100000;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::CVT_D_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the cvt.d.s instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "The destination operand of the cvt.d.s instruction must be an even-numbered register");
                }

                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b100001;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::CVT_D_W> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the cvt.d.w instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "The destination operand of the cvt.d.w instruction must be an even-numbered register");
                }

                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= 0b10100 << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b100001;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::CVT_S_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the cvt.s.d instruction");
            }
            else
            {

                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b100000;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::CVT_S_W> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the cvt.s.w instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= 0b10100 << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b100000;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::CVT_W_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the cvt.w.d instruction");
            }
            else
            {

                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b100100;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::CVT_W_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the cvt.w.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b100100;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::DIV> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg | InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the div instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegReg:
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegRegReg:
                {

                    auto a = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                    a->m_parameters.m_reg1 = params.m_reg3;
                    a->m_parameters.m_reg2 = 0;
                    a->m_parameters.m_immediate = 1;
                    //a->m_parameters.m_label = U"%NEXT_NEXT_INSTRUCTION"; //internal macro to signal the next next instruction (+8 bytes or +2 words). Must be translated to 2
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    static_cast<InstructionToken<Instruction::BREAK>*>(vec.emplace_back(new InstructionToken<Instruction::BREAK>()))->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;;
                    auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                    b->m_parameters.m_reg1 = params.m_reg2;
                    b->m_parameters.m_reg2 = params.m_reg3;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto c = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                    c->m_parameters.m_reg1 = params.m_reg1;
                    c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {

                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg2;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg2;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= 0b0000000000 << 6;
            instruction |= 0b011010;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::DIVU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg | InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the divu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegReg:
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegRegReg:
                {

                    auto a = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                    a->m_parameters.m_reg1 = params.m_reg3;
                    a->m_parameters.m_reg2 = 0;
                    a->m_parameters.m_immediate = 1;
                    //a->m_parameters.m_label = U"%NEXT_NEXT_INSTRUCTION"; //internal macro to signal not the next instruction, but the next next
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    static_cast<InstructionToken<Instruction::BREAK>*>(vec.emplace_back(new InstructionToken<Instruction::BREAK>()))->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                    b->m_parameters.m_reg1 = params.m_reg2;
                    b->m_parameters.m_reg2 = params.m_reg3;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto c = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                    c->m_parameters.m_reg1 = params.m_reg1;
                    c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;

                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg2;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg2;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= 0b0000000000 << 6;
            instruction |= 0b011011;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::DIV_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the div.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0 || params.m_reg2 % 2 != 0 || params.m_reg3 % 2 != 0)
                {
                    throw Error::InvalidSyntaxException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000011;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::DIV_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the div.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000011;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::ERET> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::NoParams))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the eret instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010000 << 26;
            instruction |= 0b1 << 25;
            instruction |= 0b0000000000000000000 << 6;
            instruction |= 0b011111;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::FLOOR_W_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the floor.w.d instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b001111;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::FLOOR_W_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the floor.w.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b001111;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::J> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::Label))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the j instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b000010 << 26;
            instruction |= m_parameters.m_resolvedLabel; //this is the address of the label. Important to note the difference from the branch instructions
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::JAL> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::Label))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the jal instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b000011 << 26;
            instruction |= m_parameters.m_resolvedLabel; //this is the address of the label. Important to note the difference from the branch instructions
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::JALR> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::Reg | InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the jalr instruction");
            }
            else
            {
                if (params.m_archetype == InstructionSyntacticArchetypes::Reg)
                {
                    params.m_reg2 = params.m_reg1;
                    params.m_reg1 = 31;
                }
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000 << 6;
            instruction |= 0b001001;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::JR> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::Label))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the jr instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= 0b000000000000000 << 6;
            instruction |= 0b001000;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::LA> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the la instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = params.m_reg2;
                        a->m_parameters.m_immediate = 0;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDIU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDIU>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_offset >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_offset & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::ADD>*>(vec.emplace_back(new InstructionToken<Instruction::ADD>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg2;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::ADD>*>(vec.emplace_back(new InstructionToken<Instruction::ADD>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg2;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::ADD>*>(vec.emplace_back(new InstructionToken<Instruction::ADD>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg2;
                        b->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::LB> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the lb instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b100000 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::LBU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the lbu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b100100 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };


    template<>
    class PseudoinstructionToken<Pseudoinstruction::LD> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the ld instruction");
            }
            else
            {
                if (params.m_reg1 == 31)
                {
                    throw Error::InvalidInstructionException("?", "Destination register for instruction ld can't be register $ra(GPR 31)");
                }
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = params.m_reg2;
                        a->m_parameters.m_offset = 0;
                        auto b = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                        b->m_parameters.m_reg1 = params.m_reg1 + 1;
                        b->m_parameters.m_reg2 = params.m_reg2;
                        b->m_parameters.m_offset = 4;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto b = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = params.m_reg2;
                            b->m_parameters.m_offset = params.m_offset;
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = (params.m_immediate + 4) >> 15;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            auto b = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                            b->m_parameters.m_reg1 = params.m_reg1 + 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = (params.m_immediate + 4) & 0xFFFF;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto d = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            d->m_parameters.m_reg1 = 1;
                            d->m_parameters.m_immediate = (params.m_offset + 4) >> 16;
                            d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto e = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            e->m_parameters.m_reg1 = 1;
                            e->m_parameters.m_reg2 = 1;
                            e->m_parameters.m_reg3 = params.m_reg2;
                            e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto f = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                            f->m_parameters.m_reg1 = params.m_reg1 + 1;
                            f->m_parameters.m_reg2 = 1;
                            f->m_parameters.m_offset = (params.m_offset + 4) & 0xFFFF;
                            f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {

                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_immediate = (params.m_immediate + 4) >> 16;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                        d->m_parameters.m_reg1 = params.m_reg1 + 1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_offset = (params.m_immediate + 4) & 0xFFFF;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_immediate = 4; //don't forget to shift it right later
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                        d->m_parameters.m_reg1 = params.m_reg1 + 1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_offset = 4;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label; // don't forget to shift right 16 bits
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_immediate = 4;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                        f->m_parameters.m_reg1 = params.m_reg1 + 1;
                        f->m_parameters.m_reg2 = 1;
                        f->m_parameters.m_offset = 4;
                        f->m_parameters.m_label = params.m_label;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_immediate = params.m_immediate + 4;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                        d->m_parameters.m_reg1 = params.m_reg1 + 1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_offset = params.m_immediate + 4;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_offset = params.m_immediate;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_immediate = params.m_immediate + 4;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        e->m_parameters.m_reg1 = 1;
                        e->m_parameters.m_reg2 = 1;
                        e->m_parameters.m_reg3 = params.m_reg2;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::LW>*>(vec.emplace_back(new InstructionToken<Instruction::LW>()));
                        f->m_parameters.m_reg1 = params.m_reg1 + 1;
                        f->m_parameters.m_reg2 = 1;
                        f->m_parameters.m_offset = params.m_immediate + 4;
                        f->m_parameters.m_label = params.m_label;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::LDC1> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the ldc1 instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label; // don't forget to shift right 16 bits
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_offset = params.m_immediate;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b110101 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::L_D> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the l.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0)
                {
                    Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }

                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LDC1>*>(vec.emplace_back(new InstructionToken<Instruction::LDC1>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = params.m_reg2;
                        a->m_parameters.m_offset = 0;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LDC1>*>(vec.emplace_back(new InstructionToken<Instruction::LDC1>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_offset = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_offset >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LDC1>*>(vec.emplace_back(new InstructionToken<Instruction::LDC1>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LDC1>*>(vec.emplace_back(new InstructionToken<Instruction::LDC1>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LDC1>*>(vec.emplace_back(new InstructionToken<Instruction::LDC1>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LDC1>*>(vec.emplace_back(new InstructionToken<Instruction::LDC1>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LDC1>*>(vec.emplace_back(new InstructionToken<Instruction::LDC1>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::L_S> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the l.s instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LWC1>*>(vec.emplace_back(new InstructionToken<Instruction::LWC1>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = params.m_reg2;
                        a->m_parameters.m_offset = 0;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LWC1>*>(vec.emplace_back(new InstructionToken<Instruction::LWC1>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_offset = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_offset >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LWC1>*>(vec.emplace_back(new InstructionToken<Instruction::LWC1>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LWC1>*>(vec.emplace_back(new InstructionToken<Instruction::LWC1>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LWC1>*>(vec.emplace_back(new InstructionToken<Instruction::LWC1>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LWC1>*>(vec.emplace_back(new InstructionToken<Instruction::LWC1>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LWC1>*>(vec.emplace_back(new InstructionToken<Instruction::LWC1>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::LH> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the lh instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b100001 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::LHU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the lhu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b100101 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::LI> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the li instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDIU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDIU>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else if (std::in_range<uint16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::LL> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the ll instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b110000 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::LUI> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the lui instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b001111 << 26;
            instruction |= 0b000000 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= m_parameters.m_immediate & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::LW> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the lw instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b100011 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::LWC1> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the lwc1 instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b110001 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::LWL> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the lwl instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b100010 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::LWR> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the lwr instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b100110 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MADD> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the madd instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b011100 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MADDU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the maddu instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b011100 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= 0b1;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MFC0> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mfc0 instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010000 << 26;
            instruction |= 0b00000 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MFC1> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mfc1 instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= 0b00000 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::MFC1_D> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mfc1.d instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken<Pseudoinstruction::MFC1_D>()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    static_cast<InstructionToken<Instruction::MFC1>*>(vec.emplace_back(new InstructionToken<Instruction::MFC1>()))->m_parameters = params;
                    auto a = static_cast<InstructionToken<Instruction::MFC1>*>(vec.emplace_back(new InstructionToken<Instruction::MFC1>()));
                    a->m_parameters.m_reg1 = params.m_reg1 + 1;
                    a->m_parameters.m_reg2 = params.m_reg2 + 1;
                    return vec;
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::MFHI> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mfhi instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b010000;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MFLO> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mflo instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b010010;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::MOVE> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the move instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken<Pseudoinstruction::MOVE>()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    auto a = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                    a->m_parameters.m_reg1 = params.m_reg1 + 1;
                    a->m_parameters.m_reg2 = 0;
                    a->m_parameters.m_reg2 = params.m_reg2;
                    return vec;
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::MOV_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mov.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0 || params.m_reg2 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                else
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };;
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000110;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MOV_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mov.s instruction");
            }
            else
            {

                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000110;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MOVF> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the movf instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params; //zero init. should take care
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_immediate << 18;
            instruction |= 0b00 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000 << 6;
            instruction |= 0b000001;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MOVF_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the movf.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0 || params.m_reg2 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                else
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params; //zero init. should take care
                    return vec;
                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= m_parameters.m_immediate << 18;
            instruction |= 0b00 << 16;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b010001;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MOVF_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the movf.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params; //zero init. should take care
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= m_parameters.m_immediate << 18;
            instruction |= 0b00 << 16;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b010001;
            return instruction;
        }
    };


    template<>
    class InstructionToken<Instruction::MOVN> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the movn instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000 << 6;
            instruction |= 0b001011;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MOVN_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the movn.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0 || params.m_reg2 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                else
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= fmt::D << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg3 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b001011;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MOVN_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the movn.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= fmt::S;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg3 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b001011;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MOVT> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the movt instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params; //zero init. should take care
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_immediate << 18;
            instruction |= 0b01 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000 << 6;
            instruction |= 0b000001;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MOVT_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the movt.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0 || params.m_reg2 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                else
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params; //zero init. should take care
                    return vec;
                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= m_parameters.m_immediate << 18;
            instruction |= 0b01 << 16;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b010001;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MOVT_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the movt.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params; //zero init. should take care
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= m_parameters.m_immediate << 18;
            instruction |= 0b01 << 16;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b010001;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MOVZ> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the movz instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000 << 6;
            instruction |= 0b001010;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MOVZ_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the movz.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0 || params.m_reg2 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                else
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= fmt::D << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg3 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b001010;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MOVZ_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the movz.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= fmt::S;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg3 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b001010;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MSUB> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the msub instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b011100 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= 0b000100;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MSUBU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the msubu instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b011100 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= 0b000101;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MTC0> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mtc0 instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010000 << 26;
            instruction |= 0b00100 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= 0b000 << 3; //'sel' parameter is not supported (yet?)
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MTC1> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mtc1 instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= 0b00100 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::MTC1_D> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mtc1.d instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    auto a = static_cast<InstructionToken<Instruction::MTC1>*>(vec.emplace_back(new InstructionToken<Instruction::MTC1>()));
                    a->m_parameters.m_reg1 = params.m_reg1;
                    a->m_parameters.m_reg2 = params.m_reg2;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::MTC1>*>(vec.emplace_back(new InstructionToken<Instruction::MTC1>()));
                    b->m_parameters.m_reg1 = params.m_reg1 + 1;
                    b->m_parameters.m_reg2 = params.m_reg2 + 1;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::MTHI> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mthi instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= 0b010001;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MTLO> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mtlo instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b010011;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MUL> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mul instruction");
            }
            else
            {
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (keepPseudoinstructions)
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    else
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_reg3 = params.m_immediate;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = params.m_reg2;
                            b->m_parameters.m_reg3 = 1;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = params.m_reg2;
                            c->m_parameters.m_reg3 = 1;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                }

                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b011100 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000;
            instruction |= 0b000010;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MUL_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mul.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0 || params.m_reg2 % 2 != 0 || params.m_reg3 % 2 == 0)
                {
                    throw Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000010;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MUL_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mul.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000010;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::MULO> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mulo instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }

                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    auto a = static_cast<InstructionToken<Instruction::MULT>*>(vec.emplace_back(new InstructionToken<Instruction::MULT>()));
                    a->m_parameters.m_reg1 = params.m_reg2;
                    a->m_parameters.m_reg2 = params.m_reg3;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::MFHI>*>(vec.emplace_back(new InstructionToken<Instruction::MFHI>()));
                    b->m_parameters.m_reg1 = 1;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto c = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                    c->m_parameters.m_reg1 = params.m_reg1;
                    c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto d = static_cast<InstructionToken<Instruction::SRA>*>(vec.emplace_back(new InstructionToken<Instruction::SRA>()));
                    d->m_parameters.m_reg1 = params.m_reg1;
                    d->m_parameters.m_reg2 = params.m_reg1;
                    d->m_parameters.m_immediate = 31;
                    d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto e = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                    e->m_parameters.m_reg1 = 1;
                    e->m_parameters.m_reg2 = params.m_reg1;
                    e->m_parameters.m_immediate = 1;
                    e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto f = static_cast<InstructionToken<Instruction::BREAK>*>(vec.emplace_back(new InstructionToken<Instruction::BREAK>()));
                    f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto g = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                    g->m_parameters.m_reg1 = params.m_reg1;
                    g->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {

                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_reg3 = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::MULT>*>(vec.emplace_back(new InstructionToken<Instruction::MULT>()));
                        b->m_parameters.m_reg1 = params.m_reg2;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::MFHI>*>(vec.emplace_back(new InstructionToken<Instruction::MFHI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::SRA>*>(vec.emplace_back(new InstructionToken<Instruction::SRA>()));
                        e->m_parameters.m_reg1 = params.m_reg1;
                        e->m_parameters.m_reg2 = params.m_reg1;
                        e->m_parameters.m_immediate = 31;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                        f->m_parameters.m_reg1 = 1;
                        f->m_parameters.m_reg2 = params.m_reg1;
                        f->m_parameters.m_immediate = 1;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto g = static_cast<InstructionToken<Instruction::BREAK>*>(vec.emplace_back(new InstructionToken<Instruction::BREAK>()));
                        g->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto h = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                        h->m_parameters.m_reg1 = params.m_reg1;
                        h->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::MULT>*>(vec.emplace_back(new InstructionToken<Instruction::MULT>()));
                        c->m_parameters.m_reg1 = params.m_reg2;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::MFHI>*>(vec.emplace_back(new InstructionToken<Instruction::MFHI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                        e->m_parameters.m_reg1 = params.m_reg1;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::SRA>*>(vec.emplace_back(new InstructionToken<Instruction::SRA>()));
                        f->m_parameters.m_reg1 = params.m_reg1;
                        f->m_parameters.m_reg2 = params.m_reg1;
                        f->m_parameters.m_immediate = 31;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto g = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                        g->m_parameters.m_reg1 = 1;
                        g->m_parameters.m_reg2 = params.m_reg1;
                        g->m_parameters.m_immediate = 1;
                        g->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto h = static_cast<InstructionToken<Instruction::BREAK>*>(vec.emplace_back(new InstructionToken<Instruction::BREAK>()));
                        h->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto i = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                        i->m_parameters.m_reg1 = params.m_reg1;
                        i->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }

                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::MULOU> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mulou instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }

                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    auto a = static_cast<InstructionToken<Instruction::MULTU>*>(vec.emplace_back(new InstructionToken<Instruction::MULTU>()));
                    a->m_parameters.m_reg1 = params.m_reg2;
                    a->m_parameters.m_reg2 = params.m_reg3;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::MFHI>*>(vec.emplace_back(new InstructionToken<Instruction::MFHI>()));
                    b->m_parameters.m_reg1 = 1;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto c = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                    c->m_parameters.m_reg1 = 1;
                    c->m_parameters.m_reg2 = 0;
                    c->m_parameters.m_immediate = 1;
                    c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto d = static_cast<InstructionToken<Instruction::BREAK>*>(vec.emplace_back(new InstructionToken<Instruction::BREAK>()));
                    d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto e = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                    e->m_parameters.m_reg1 = params.m_reg1;
                    e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {

                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_reg3 = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::MULTU>*>(vec.emplace_back(new InstructionToken<Instruction::MULTU>()));
                        b->m_parameters.m_reg1 = params.m_reg2;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::MFHI>*>(vec.emplace_back(new InstructionToken<Instruction::MFHI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = 0;
                        d->m_parameters.m_immediate = 1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::BREAK>*>(vec.emplace_back(new InstructionToken<Instruction::BREAK>()));
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                        f->m_parameters.m_reg1 = params.m_reg1;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::MULTU>*>(vec.emplace_back(new InstructionToken<Instruction::MULTU>()));
                        c->m_parameters.m_reg1 = params.m_reg2;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::MFHI>*>(vec.emplace_back(new InstructionToken<Instruction::MFHI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::BEQ>*>(vec.emplace_back(new InstructionToken<Instruction::BEQ>()));
                        e->m_parameters.m_reg1 = 1;
                        e->m_parameters.m_reg2 = 0;
                        e->m_parameters.m_immediate = 1;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::BREAK>*>(vec.emplace_back(new InstructionToken<Instruction::BREAK>()));
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto g = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                        g->m_parameters.m_reg1 = params.m_reg1;
                        g->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }

                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::MULT> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mult instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= 0b011000;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::MULTU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the multu instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= 0b011001;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::MULU> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the mulu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    auto a = static_cast<InstructionToken<Instruction::MULTU>*>(vec.emplace_back(new InstructionToken<Instruction::MULTU>()));
                    a->m_parameters.m_reg1 = params.m_reg2;
                    a->m_parameters.m_reg2 = params.m_reg3;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                    b->m_parameters.m_reg1 = params.m_reg1;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {

                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_reg3 = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto a = static_cast<InstructionToken<Instruction::MULTU>*>(vec.emplace_back(new InstructionToken<Instruction::MULTU>()));
                        a->m_parameters.m_reg1 = params.m_reg2;
                        a->m_parameters.m_reg2 = 1;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::MULTU>*>(vec.emplace_back(new InstructionToken<Instruction::MULTU>()));
                        c->m_parameters.m_reg1 = params.m_reg2;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::MFLO>*>(vec.emplace_back(new InstructionToken<Instruction::MFLO>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }

                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::NEG> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the neg instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                auto a = static_cast<InstructionToken<Instruction::SUB>*>(vec.emplace_back(new InstructionToken<Instruction::SUB>()));
                a->m_parameters.m_reg1 = params.m_reg1;
                a->m_parameters.m_reg2 = 0;
                a->m_parameters.m_reg3 = params.m_reg2;
                return vec;
            }
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::NEGU> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the negu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                auto a = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                a->m_parameters.m_reg1 = params.m_reg1;
                a->m_parameters.m_reg2 = 0;
                a->m_parameters.m_reg3 = params.m_reg2;
                return vec;
            }
        }
    };

    template<>
    class InstructionToken<Instruction::NEG_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the neg.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0 || params.m_reg2 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b000111;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::NEG_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the neg.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b000111;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::NOP> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::NoParams))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the nop instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::NOR> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the nor instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b100111;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::NOT> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the not instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    auto a = static_cast<InstructionToken<Instruction::NOR>*>(vec.emplace_back(new InstructionToken<Instruction::NOR>()));
                    a->m_parameters.m_reg1 = params.m_reg1;
                    a->m_parameters.m_reg2 = params.m_reg2;
                    a->m_parameters.m_reg3 = 0;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::OR> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm | InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the or instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else 
                {
                    if (!std::in_range<uint16_t>(params.m_immediate))
                    {
                        throw Error::InvalidInstructionException("?", "immediate for or instruction must be in the unsigned 16 bit int range");
                    }
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegRegReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegRegImm:
                    {
                        static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = params.m_reg1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        return vec;
                    }
                    }
                }

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b100101;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::ORI> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegImm | InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the ori instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegRegImm:
                    {
                        if (std::in_range<uint16_t>(params.m_immediate))
                        {
                            static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = params.m_reg2;
                            c->m_parameters.m_reg3 = 1;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<uint16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = params.m_reg1;
                            a->m_parameters.m_immediate = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = params.m_reg1;
                            c->m_parameters.m_reg3 = 1;
                            return vec;
                        }
                    }
                    }
                }

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b001101 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= m_parameters.m_immediate & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::ROUND_W_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the round.w.d instruction");
            }
            else
            {
                if (params.m_reg2 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "The destination operand of the round.w.d instruction must be an even-numbered register");
                }

                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::COP1 << 26;
            instruction |= fmt::S << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b001100;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::ROUND_W_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the round.w.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::COP1 << 26;
            instruction |= fmt::S << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b001100;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::REM> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the rem instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegRegReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                        a->m_parameters.m_reg1 = params.m_reg3;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = 1;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::BREAK>*>(vec.emplace_back(new InstructionToken<Instruction::BREAK>()));
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::DIV>*>(vec.emplace_back(new InstructionToken<Instruction::DIV>()));
                        c->m_parameters.m_reg1 = params.m_reg2;
                        c->m_parameters.m_reg2 = params.m_reg3;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::MFHI>*>(vec.emplace_back(new InstructionToken<Instruction::MFHI>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegRegImm:
                    {

                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_reg3 = params.m_immediate;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::DIV>*>(vec.emplace_back(new InstructionToken<Instruction::DIV>()));
                            b->m_parameters.m_reg1 = params.m_reg2;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::MFHI>*>(vec.emplace_back(new InstructionToken<Instruction::MFHI>()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::DIV>*>(vec.emplace_back(new InstructionToken<Instruction::DIV>()));
                            c->m_parameters.m_reg1 = params.m_reg2;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto d = static_cast<InstructionToken<Instruction::MFHI>*>(vec.emplace_back(new InstructionToken<Instruction::MFHI>()));
                            d->m_parameters.m_reg1 = params.m_reg1;
                            d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }

                    }

                    }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::REMU> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the remu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegRegReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::BNE>*>(vec.emplace_back(new InstructionToken<Instruction::BNE>()));
                        a->m_parameters.m_reg1 = params.m_reg3;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = 1;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::BREAK>*>(vec.emplace_back(new InstructionToken<Instruction::BREAK>()));
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::DIVU>*>(vec.emplace_back(new InstructionToken<Instruction::DIVU>()));
                        c->m_parameters.m_reg1 = params.m_reg2;
                        c->m_parameters.m_reg2 = params.m_reg3;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::MFHI>*>(vec.emplace_back(new InstructionToken<Instruction::MFHI>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegRegImm:
                    {

                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_reg3 = params.m_immediate;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::DIVU>*>(vec.emplace_back(new InstructionToken<Instruction::DIVU>()));
                            b->m_parameters.m_reg1 = params.m_reg2;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::MFHI>*>(vec.emplace_back(new InstructionToken<Instruction::MFHI>()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::DIVU>*>(vec.emplace_back(new InstructionToken<Instruction::DIVU>()));
                            c->m_parameters.m_reg1 = params.m_reg2;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto d = static_cast<InstructionToken<Instruction::MFHI>*>(vec.emplace_back(new InstructionToken<Instruction::MFHI>()));
                            d->m_parameters.m_reg1 = params.m_reg1;
                            d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }

                    }

                    }
                }
            }
        }
    };


    template<>
    class PseudoinstructionToken<Pseudoinstruction::ROL> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the rol instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegRegReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_reg3 = params.m_reg3;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SRLV>*>(vec.emplace_back(new InstructionToken<Instruction::SRLV>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = params.m_reg2;
                        b->m_parameters.m_reg3 = 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLLV>*>(vec.emplace_back(new InstructionToken<Instruction::SLLV>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg2;
                        c->m_parameters.m_reg3 = params.m_reg3;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_reg2 = params.m_reg1;
                        d->m_parameters.m_reg3 = 1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegRegImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::SRL>*>(vec.emplace_back(new InstructionToken<Instruction::SRLV>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = params.m_reg2;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLLV>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = params.m_reg2;
                        b->m_parameters.m_immediate = params.m_immediate;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg1;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                       

                    }

                    }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::ROR> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the ror instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegRegReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_reg3 = params.m_reg3;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SLLV>*>(vec.emplace_back(new InstructionToken<Instruction::SLLV>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = params.m_reg2;
                        b->m_parameters.m_reg3 = 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SRLV>*>(vec.emplace_back(new InstructionToken<Instruction::SRLV>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg2;
                        c->m_parameters.m_reg3 = params.m_reg3;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_reg2 = params.m_reg1;
                        d->m_parameters.m_reg3 = 1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegRegImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = params.m_reg2;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SRL>*>(vec.emplace_back(new InstructionToken<Instruction::SRL>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = params.m_reg2;
                        b->m_parameters.m_immediate = params.m_immediate;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg1;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                    }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::S_D> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the s.d instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0)
                {
                    Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }

                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::SDC1>*>(vec.emplace_back(new InstructionToken<Instruction::SDC1>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = params.m_reg2;
                        a->m_parameters.m_offset = 0;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::SDC1>*>(vec.emplace_back(new InstructionToken<Instruction::SDC1>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_offset = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_offset >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SDC1>*>(vec.emplace_back(new InstructionToken<Instruction::SDC1>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SDC1>*>(vec.emplace_back(new InstructionToken<Instruction::SDC1>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SDC1>*>(vec.emplace_back(new InstructionToken<Instruction::SDC1>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SDC1>*>(vec.emplace_back(new InstructionToken<Instruction::SDC1>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SDC1>*>(vec.emplace_back(new InstructionToken<Instruction::SDC1>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::S_S> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the s.s instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::SWC1>*>(vec.emplace_back(new InstructionToken<Instruction::SWC1>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = params.m_reg2;
                        a->m_parameters.m_offset = 0;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::SWC1>*>(vec.emplace_back(new InstructionToken<Instruction::SWC1>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_offset = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_offset >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SWC1>*>(vec.emplace_back(new InstructionToken<Instruction::SWC1>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SWC1>*>(vec.emplace_back(new InstructionToken<Instruction::SWC1>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SWC1>*>(vec.emplace_back(new InstructionToken<Instruction::SWC1>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SWC1>*>(vec.emplace_back(new InstructionToken<Instruction::SWC1>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SWC1>*>(vec.emplace_back(new InstructionToken<Instruction::SWC1>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::SB> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sb instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b101000 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SC> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sc instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            //before Release 6 the opcode is 0b111000. Starting in release 6 it's 011111
            instruction |= 0b111000 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::SD> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sd instruction");
            }
            else
            {
                if (params.m_reg1 == 31)
                {
                    throw Error::InvalidInstructionException("?", "Destination register for instruction sd can't be register $ra(GPR 31)");
                }
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = params.m_reg2;
                        a->m_parameters.m_offset = 0;
                        auto b = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                        b->m_parameters.m_reg1 = params.m_reg1 + 1;
                        b->m_parameters.m_reg2 = params.m_reg2;
                        b->m_parameters.m_offset = 4;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto b = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = params.m_reg2;
                            b->m_parameters.m_offset = params.m_offset;
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = (params.m_immediate + 4) >> 15;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            auto b = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                            b->m_parameters.m_reg1 = params.m_reg1 + 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = (params.m_immediate + 4) & 0xFFFF;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto d = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            d->m_parameters.m_reg1 = 1;
                            d->m_parameters.m_immediate = (params.m_offset + 4) >> 16;
                            d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto e = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            e->m_parameters.m_reg1 = 1;
                            e->m_parameters.m_reg2 = 1;
                            e->m_parameters.m_reg3 = params.m_reg2;
                            e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto f = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                            f->m_parameters.m_reg1 = params.m_reg1 + 1;
                            f->m_parameters.m_reg2 = 1;
                            f->m_parameters.m_offset = (params.m_offset + 4) & 0xFFFF;
                            f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {

                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_immediate = (params.m_immediate + 4) >> 16;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                        d->m_parameters.m_reg1 = params.m_reg1 + 1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_offset = (params.m_immediate + 4) & 0xFFFF;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_immediate = 4; //don't forget to shift it right later
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                        d->m_parameters.m_reg1 = params.m_reg1 + 1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_offset = 4;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label; // don't forget to shift right 16 bits
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_immediate = 4;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                        f->m_parameters.m_reg1 = params.m_reg1 + 1;
                        f->m_parameters.m_reg2 = 1;
                        f->m_parameters.m_offset = 4;
                        f->m_parameters.m_label = params.m_label;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_immediate = params.m_immediate + 4;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                        d->m_parameters.m_reg1 = params.m_reg1 + 1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_offset = params.m_immediate + 4;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_offset = params.m_immediate;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_immediate = params.m_immediate + 4;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        e->m_parameters.m_reg1 = 1;
                        e->m_parameters.m_reg2 = 1;
                        e->m_parameters.m_reg3 = params.m_reg2;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::SW>*>(vec.emplace_back(new InstructionToken<Instruction::SW>()));
                        f->m_parameters.m_reg1 = params.m_reg1 + 1;
                        f->m_parameters.m_reg2 = 1;
                        f->m_parameters.m_offset = params.m_immediate + 4;
                        f->m_parameters.m_label = params.m_label;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::SDC1> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sdc1 instruction");
            }
            else
            {
                if (params.m_reg1 % 2 != 0)
                {
                    throw Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
                }
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label; // don't forget to shift right 16 bits
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_offset = params.m_immediate;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b111101 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };


    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::SEQ> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the seq instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    auto a = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                    a->m_parameters.m_reg1 = params.m_reg1;
                    a->m_parameters.m_reg2 = params.m_reg2;
                    a->m_parameters.m_reg3 = params.m_reg3;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                    a->m_parameters.m_reg1 = 1;
                    a->m_parameters.m_reg2 = 1;
                    a->m_parameters.m_immediate = 1;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto c = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                    c->m_parameters.m_reg1 = params.m_reg1;
                    c->m_parameters.m_reg2 = params.m_reg1;
                    c->m_parameters.m_reg3 = 1;
                    c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;

                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = params.m_reg2;
                        b->m_parameters.m_reg3 = 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_immediate = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_reg2 = params.m_reg1;
                        d->m_parameters.m_reg3 = 1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg2;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_immediate = 1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        e->m_parameters.m_reg1 = params.m_reg1;
                        e->m_parameters.m_reg2 = params.m_reg1;
                        e->m_parameters.m_reg3 = 1;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::SGE> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sge instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    auto a = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                    a->m_parameters.m_reg1 = params.m_reg1;
                    a->m_parameters.m_reg2 = params.m_reg2;
                    a->m_parameters.m_reg3 = params.m_reg3;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                    a->m_parameters.m_reg1 = 1;
                    a->m_parameters.m_reg2 = 1;
                    a->m_parameters.m_immediate = 1;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto c = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                    c->m_parameters.m_reg1 = params.m_reg1;
                    c->m_parameters.m_reg2 = 1;
                    c->m_parameters.m_reg3 = params.m_reg1;
                    c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;

                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = params.m_reg2;
                        b->m_parameters.m_reg3 = 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_immediate = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_reg3 = params.m_reg1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg2;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_immediate = 1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        e->m_parameters.m_reg1 = params.m_reg1;
                        e->m_parameters.m_reg2 = 1;
                        e->m_parameters.m_reg3 = params.m_reg1;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::SGEU> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sgeu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    auto a = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                    a->m_parameters.m_reg1 = params.m_reg1;
                    a->m_parameters.m_reg2 = params.m_reg2;
                    a->m_parameters.m_reg3 = params.m_reg3;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                    a->m_parameters.m_reg1 = 1;
                    a->m_parameters.m_reg2 = 1;
                    a->m_parameters.m_immediate = 1;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto c = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                    c->m_parameters.m_reg1 = params.m_reg1;
                    c->m_parameters.m_reg2 = 1;
                    c->m_parameters.m_reg3 = params.m_reg1;
                    c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;

                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = params.m_reg2;
                        b->m_parameters.m_reg3 = 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_immediate = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_reg3 = params.m_reg1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg2;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_immediate = 1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        e->m_parameters.m_reg1 = params.m_reg1;
                        e->m_parameters.m_reg2 = 1;
                        e->m_parameters.m_reg3 = params.m_reg1;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::SGT> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sgt instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    auto a = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                    a->m_parameters.m_reg1 = params.m_reg1;
                    a->m_parameters.m_reg2 = params.m_reg3;
                    a->m_parameters.m_reg3 = params.m_reg2;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;

                    return vec;

                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_reg3 = params.m_reg2;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };


    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::SGTU> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sgtu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    auto a = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                    a->m_parameters.m_reg1 = params.m_reg1;
                    a->m_parameters.m_reg2 = params.m_reg3;
                    a->m_parameters.m_reg3 = params.m_reg2;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;

                    return vec;

                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_reg3 = params.m_reg2;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };


    template<>
    class InstructionToken<Instruction::SH> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sh instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b101001 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };


    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::SLE> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sle instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    auto a = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                    a->m_parameters.m_reg1 = params.m_reg1;
                    a->m_parameters.m_reg2 = params.m_reg3;
                    a->m_parameters.m_reg3 = params.m_reg2;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                    b->m_parameters.m_reg1 = 1;
                    b->m_parameters.m_reg2 = 1;
                    b->m_parameters.m_immediate = 1;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto c = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                    c->m_parameters.m_reg1 = params.m_reg1;
                    c->m_parameters.m_reg2 = 1;
                    c->m_parameters.m_reg3 = params.m_reg1;
                    c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;

                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_immediate = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_reg3 = params.m_reg1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLT>*>(vec.emplace_back(new InstructionToken<Instruction::SLT>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_reg3 = params.m_reg2;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_immediate = 1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        e->m_parameters.m_reg1 = params.m_reg1;
                        e->m_parameters.m_reg2 = 1;
                        e->m_parameters.m_reg3 = params.m_reg1;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };


    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::SLEU> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sleu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    auto a = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                    a->m_parameters.m_reg1 = params.m_reg1;
                    a->m_parameters.m_reg2 = params.m_reg3;
                    a->m_parameters.m_reg3 = params.m_reg2;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                    b->m_parameters.m_reg1 = 1;
                    b->m_parameters.m_reg2 = 1;
                    b->m_parameters.m_immediate = 1;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto c = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                    c->m_parameters.m_reg1 = params.m_reg1;
                    c->m_parameters.m_reg2 = 1;
                    c->m_parameters.m_reg3 = params.m_reg1;
                    c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;

                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_immediate = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_reg3 = params.m_reg1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_reg3 = params.m_reg2;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = 1;
                        d->m_parameters.m_immediate = 1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        e->m_parameters.m_reg1 = params.m_reg1;
                        e->m_parameters.m_reg2 = 1;
                        e->m_parameters.m_reg3 = params.m_reg1;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::SLL> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sll instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= 0b00000 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= m_parameters.m_immediate << 6;
            instruction |= 0b000000;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SLLV> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sllv instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg3 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000 << 6;
            instruction |= 0b000100;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SLT> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the slt instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b101010;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SLTI> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the slti instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b001010 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= m_parameters.m_immediate;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SLTIU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sltiu instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b001011 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= m_parameters.m_immediate;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SLTU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sltu instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b101011;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SQRT_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sqrt.d instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::D << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000100;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SQRT_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sqrt.s instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b010001 << 26;
            instruction |= fmt::S << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000100;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SRL> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the srl instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= 0b00000 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= m_parameters.m_immediate << 6;
            instruction |= 0b000010;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SRLV> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the srlv instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg3 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000 << 6;
            instruction |= 0b000110;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<ISA::Pseudoinstruction::SNE> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sne instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    auto a = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                    a->m_parameters.m_reg1 = params.m_reg1;
                    a->m_parameters.m_reg2 = params.m_reg3;
                    a->m_parameters.m_reg3 = params.m_reg2;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                    b->m_parameters.m_reg1 = params.m_reg1;
                    b->m_parameters.m_reg2 = 0;
                    b->m_parameters.m_reg3 = params.m_reg1;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;

                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (std::in_range<int16_t>(params.m_immediate))
                    {
                        auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_reg2 = 0;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = params.m_reg2;
                        b->m_parameters.m_reg3 = 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 0;
                        c->m_parameters.m_reg3 = params.m_reg3;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg2;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::SLTU>*>(vec.emplace_back(new InstructionToken<Instruction::SLTU>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_reg2 = 0;
                        d->m_parameters.m_reg3 = params.m_reg3;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }

                }
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::SRA> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sra instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= 0b00000 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg3 << 11;
            instruction |= m_parameters.m_immediate << 6;
            instruction |= 0b000011;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SRAV> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the srav instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg3 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000 << 6;
            instruction |= 0b000111;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SUB> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sub instruction");
            }
            else
            {
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (keepPseudoinstructions)
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    else
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_immediate = params.m_immediate;
                            auto b = static_cast<InstructionToken<Instruction::SUB>*>(vec.emplace_back(new InstructionToken<Instruction::SUB>()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = params.m_reg2;
                            b->m_parameters.m_reg3 = 1;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::SUB>*>(vec.emplace_back(new InstructionToken<Instruction::SUB>()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = params.m_reg2;
                            c->m_parameters.m_reg3 = 1;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                }

                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000;
            instruction |= 0b100010;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SUB_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            auto i = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
            if (!parse_instruction(instructionStr, i->m_parameters, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sub.d instruction");
            }
            if (i->m_parameters.m_reg1 % 2 != 0 || i->m_parameters.m_reg2 % 2 != 0 || i->m_parameters.m_reg3 % 2 != 0)
            {
                throw  Error::InvalidInstructionException("?", "64 bit floating point instructions must use even-numbered registers");
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::COP1 << 26;
            instruction |= fmt::D << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000001;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SUB_S> : public InstructionTokenBase
    {
        InstructionToken() : InstructionTokenBase() {}
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            auto i = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
            if (!parse_instruction(instructionStr, i->m_parameters, InstructionSyntacticArchetypes::RegRegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sub.s instruction");
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::COP1 << 26;
            instruction |= fmt::S << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b000001;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::SUBI> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the subi instruction");
            }
            else
            {
                if (std::in_range<int16_t>(params.m_immediate))
                {
                    auto a = static_cast<InstructionToken<Instruction::ADDI>*>(vec.emplace_back(new InstructionToken<Instruction::ADDI>()));
                    a->m_parameters.m_reg1 = params.m_reg1;
                    a->m_parameters.m_reg2 = 0;
                    a->m_parameters.m_immediate = params.m_immediate;
                    auto b = static_cast<InstructionToken<Instruction::SUB>*>(vec.emplace_back(new InstructionToken<Instruction::SUB>()));
                    b->m_parameters.m_reg1 = params.m_reg1;
                    b->m_parameters.m_reg2 = params.m_reg2;
                    b->m_parameters.m_reg3 = 1;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;
                }
                else if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                    a->m_parameters.m_reg1 = 1;
                    a->m_parameters.m_immediate = params.m_immediate >> 16;
                    a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                    b->m_parameters.m_reg1 = 1;
                    b->m_parameters.m_reg2 = 1;
                    b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                    b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    auto c = static_cast<InstructionToken<Instruction::SUB>*>(vec.emplace_back(new InstructionToken<Instruction::SUB>()));
                    c->m_parameters.m_reg1 = params.m_reg1;
                    c->m_parameters.m_reg2 = params.m_reg2;
                    c->m_parameters.m_reg3 = 1;
                    c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                    return vec;
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::SUBIU> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the subiu instruction");
            }
            if (keepPseudoinstructions)
            {
                static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                return vec;
            }
            else
            {
                auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                a->m_parameters.m_reg1 = 1;
                a->m_parameters.m_immediate = params.m_immediate >> 16;
                a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                b->m_parameters.m_reg1 = 1;
                b->m_parameters.m_reg2 = 1;
                b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                auto c = static_cast<InstructionToken<Instruction::SUBU>*>(vec.emplace_back(new InstructionToken<Instruction::SUBU>()));
                c->m_parameters.m_reg1 = params.m_reg1;
                c->m_parameters.m_reg2 = params.m_reg2;
                c->m_parameters.m_reg3 = 1;
                c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                return vec;
            }
        }
    };

    template<>
    class InstructionToken<Instruction::SUBU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the subu instruction");
            }
            else
            {
                switch (params.m_archetype)
                {
                case InstructionSyntacticArchetypes::RegRegReg:
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                case InstructionSyntacticArchetypes::RegRegImm:
                {
                    if (keepPseudoinstructions)
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    else
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SUB>*>(vec.emplace_back(new InstructionToken<Instruction::SUB>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg2;
                        c->m_parameters.m_reg3 = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;

                    }
                }

                }
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b00000;
            instruction |= 0b100011;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SW> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the sw instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b101011 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SWC1> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the swc1 instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b111001 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SWL> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the swl instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b101010 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SWR> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the swr instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate)) //real instruction version
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = 0;
                            a->m_parameters.m_offset = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                            b->m_parameters.m_reg1 = params.m_reg1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_offset = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_immediate; //the sum of the address of the label and and imm is done in ResolveLabel and the high 16 bits are extracted there
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = params.m_immediate; //the sum of the address of the label and and imm is done in Encode and the high 16 bits are extracted there
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }

        void ResolveLabel(const std::unordered_map<std::u32string_view, uint32_t>& table, uint32_t currentMemPos) override
        {
            m_parameters.m_resolvedLabel = m_parameters.m_label.empty() ? 0 : table.at(m_parameters.m_label);
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            uint32_t address = m_parameters.m_resolvedLabel + m_parameters.m_offset;
            instruction |= 0b101110 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= address & 0xFFFF;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::SYSCALL> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::NoParams))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the syscall instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_immediate << 6;
            instruction |= 0b001100;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::TEQ> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the teq instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            //there are 10 bits here avaiable for software implementations. To access them, read the instruction from memory
            instruction |= 0b110100;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::TEQI> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the teqi instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b000001 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= 0b01100 << 16;
            instruction |= m_parameters.m_immediate;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::TGE> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the tge instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            //there are 10 bits here avaiable for software implementations. To access them, read the instruction from memory
            instruction |= 0b110000;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::TGEI> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the tgei instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b000001 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= 0b01000 << 16;
            instruction |= m_parameters.m_immediate;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::TGEIU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the tgeiu instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b000001 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= 0b01001 << 16;
            instruction |= m_parameters.m_immediate;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::TGEU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the tgeu instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            //there are 10 bits here avaiable for software implementations. To access them, read the instruction from memory
            instruction |= 0b110001;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::TLT> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the tlt instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            //there are 10 bits here avaiable for software implementations. To access them, read the instruction from memory
            instruction |= 0b110010;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::TLTI> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the tlti instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b000001 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= 0b01010 << 16;
            instruction |= m_parameters.m_immediate;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::TLTIU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the tltiu instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b000001 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= 0b01011 << 16;
            instruction |= m_parameters.m_immediate;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::TLTU> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the tltu instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            //there are 10 bits here avaiable for software implementations. To access them, read the instruction from memory
            instruction |= 0b110011;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::TNE> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the tne instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= m_parameters.m_reg2 << 16;
            //there are 10 bits here avaiable for software implementations. To access them, read the instruction from memory
            instruction |= 0b110110;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::TNEI> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the tnei instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b000001 << 26;
            instruction |= m_parameters.m_reg1 << 21;
            instruction |= 0b01110 << 16;
            instruction |= m_parameters.m_immediate;
            return instruction;
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::ULH> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the ulh instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::LB>*>(vec.emplace_back(new InstructionToken<Instruction::LB>()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset + 1;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto d = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                            d->m_parameters.m_reg1 = 1;
                            d->m_parameters.m_reg2 = params.m_reg2;
                            d->m_parameters.m_offset = params.m_offset;
                            d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto e = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                            e->m_parameters.m_reg1 = params.m_reg1;
                            e->m_parameters.m_reg2 = params.m_reg1;
                            e->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                            e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto f = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                            f->m_parameters.m_reg1 = params.m_reg1;
                            f->m_parameters.m_reg2 = params.m_reg1;
                            f->m_parameters.m_reg3 = 1;
                            f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::LB>*>(vec.emplace_back(new InstructionToken<Instruction::LB>()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = (params.m_offset & 0xFFFF) + 1;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto d = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            d->m_parameters.m_reg1 = 1;
                            d->m_parameters.m_immediate = params.m_offset >> 16;
                            d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto e = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            e->m_parameters.m_reg1 = 1;
                            e->m_parameters.m_reg2 = 1;
                            e->m_parameters.m_reg3 = params.m_reg2;
                            e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto f = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                            f->m_parameters.m_reg1 = 1;
                            f->m_parameters.m_reg2 = params.m_reg2;
                            f->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto g = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                            g->m_parameters.m_reg1 = params.m_reg1;
                            g->m_parameters.m_reg2 = params.m_reg1;
                            g->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                            g->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto h = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                            h->m_parameters.m_reg1 = params.m_reg1;
                            h->m_parameters.m_reg2 = params.m_reg1;
                            h->m_parameters.m_reg3 = 1;
                            h->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LB>*>(vec.emplace_back(new InstructionToken<Instruction::LB>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = params.m_reg2;
                        a->m_parameters.m_offset = 1;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = params.m_reg2;
                        b->m_parameters.m_offset = 0;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg1;
                        c->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_reg2 = params.m_reg1;
                        d->m_parameters.m_reg3 = 1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_offset >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LB>*>(vec.emplace_back(new InstructionToken<Instruction::LB>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = (params.m_offset & 0xFFFF) + 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_immediate = params.m_offset >> 16;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = params.m_reg2;
                        d->m_parameters.m_offset = params.m_offset & 0xFFFF;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                        e->m_parameters.m_reg1 = params.m_reg1;
                        e->m_parameters.m_reg2 = params.m_reg1;
                        e->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        f->m_parameters.m_reg1 = params.m_reg1;
                        f->m_parameters.m_reg2 = params.m_reg1;
                        f->m_parameters.m_reg3 = 1;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LB>*>(vec.emplace_back(new InstructionToken<Instruction::LB>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_offset = 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = params.m_reg2;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                        e->m_parameters.m_reg1 = params.m_reg1;
                        e->m_parameters.m_reg2 = params.m_reg1;
                        e->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        f->m_parameters.m_reg1 = params.m_reg1;
                        f->m_parameters.m_reg2 = params.m_reg1;
                        f->m_parameters.m_reg3 = 1;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LB>*>(vec.emplace_back(new InstructionToken<Instruction::LB>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_offset = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        e->m_parameters.m_reg1 = 1;
                        e->m_parameters.m_reg2 = 1;
                        e->m_parameters.m_reg3 = params.m_reg2;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        f->m_parameters.m_reg1 = 1;
                        f->m_parameters.m_reg2 = params.m_reg2;
                        f->m_parameters.m_label = params.m_label;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto g = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                        g->m_parameters.m_reg1 = params.m_reg1;
                        g->m_parameters.m_reg2 = params.m_reg1;
                        g->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                        g->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto h = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        h->m_parameters.m_reg1 = params.m_reg1;
                        h->m_parameters.m_reg2 = params.m_reg1;
                        h->m_parameters.m_reg3 = 1;
                        h->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LB>*>(vec.emplace_back(new InstructionToken<Instruction::LB>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_offset = params.m_immediate + 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_immediate = params.m_immediate;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = params.m_reg2;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_offset = params.m_immediate;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                        e->m_parameters.m_reg1 = params.m_reg1;
                        e->m_parameters.m_reg2 = params.m_reg1;
                        e->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        f->m_parameters.m_reg1 = params.m_reg1;
                        f->m_parameters.m_reg2 = params.m_reg1;
                        f->m_parameters.m_reg3 = 1;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LB>*>(vec.emplace_back(new InstructionToken<Instruction::LB>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_offset = params.m_immediate + 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_immediate = params.m_immediate;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        e->m_parameters.m_reg1 = 1;
                        e->m_parameters.m_reg2 = 1;
                        e->m_parameters.m_reg3 = params.m_reg2;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        f->m_parameters.m_reg1 = 1;
                        f->m_parameters.m_reg2 = params.m_reg2;
                        f->m_parameters.m_offset = params.m_immediate;
                        f->m_parameters.m_label = params.m_label;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto g = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                        g->m_parameters.m_reg1 = params.m_reg1;
                        g->m_parameters.m_reg2 = params.m_reg1;
                        g->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                        g->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto h = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        h->m_parameters.m_reg1 = params.m_reg1;
                        h->m_parameters.m_reg2 = params.m_reg1;
                        h->m_parameters.m_reg3 = 1;
                        h->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }
    };

    template<>
    class PseudoinstructionToken<Pseudoinstruction::ULHU> : public PseudoinstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegMemReg | InstructionSyntacticArchetypes::RegImm | InstructionSyntacticArchetypes::RegOffsetForReg | InstructionSyntacticArchetypes::RegLabel | InstructionSyntacticArchetypes::RegLabelAsOffsetReg | InstructionSyntacticArchetypes::RegLabelPlusImm | InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the ulhu instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<PseudoinstructionToken*>(vec.emplace_back(new PseudoinstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegOffsetForReg:
                    {
                        if (std::in_range<int16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = params.m_offset + 1;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto d = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                            d->m_parameters.m_reg1 = 1;
                            d->m_parameters.m_reg2 = params.m_reg2;
                            d->m_parameters.m_offset = params.m_offset;
                            d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto e = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                            e->m_parameters.m_reg1 = params.m_reg1;
                            e->m_parameters.m_reg2 = params.m_reg1;
                            e->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                            e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto f = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                            f->m_parameters.m_reg1 = params.m_reg1;
                            f->m_parameters.m_reg2 = params.m_reg1;
                            f->m_parameters.m_reg3 = 1;
                            f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_offset >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_reg3 = params.m_reg2;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = 1;
                            c->m_parameters.m_offset = (params.m_offset & 0xFFFF) + 1;
                            c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto d = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            d->m_parameters.m_reg1 = 1;
                            d->m_parameters.m_immediate = params.m_offset >> 16;
                            d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto e = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                            e->m_parameters.m_reg1 = 1;
                            e->m_parameters.m_reg2 = 1;
                            e->m_parameters.m_reg3 = params.m_reg2;
                            e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto f = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                            f->m_parameters.m_reg1 = 1;
                            f->m_parameters.m_reg2 = params.m_reg2;
                            f->m_parameters.m_offset = params.m_offset & 0xFFFF;
                            f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto g = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                            g->m_parameters.m_reg1 = params.m_reg1;
                            g->m_parameters.m_reg2 = params.m_reg1;
                            g->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                            g->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto h = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                            h->m_parameters.m_reg1 = params.m_reg1;
                            h->m_parameters.m_reg2 = params.m_reg1;
                            h->m_parameters.m_reg3 = 1;
                            h->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            return vec;
                        }
                    }
                    case  InstructionSyntacticArchetypes::RegMemReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = params.m_reg2;
                        a->m_parameters.m_offset = 1;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = params.m_reg2;
                        b->m_parameters.m_offset = 0;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = params.m_reg1;
                        c->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        d->m_parameters.m_reg1 = params.m_reg1;
                        d->m_parameters.m_reg2 = params.m_reg1;
                        d->m_parameters.m_reg3 = 1;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_immediate = params.m_offset >> 16;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_offset = (params.m_offset & 0xFFFF) + 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_immediate = params.m_offset >> 16;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = params.m_reg2;
                        d->m_parameters.m_offset = params.m_offset & 0xFFFF;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                        e->m_parameters.m_reg1 = params.m_reg1;
                        e->m_parameters.m_reg2 = params.m_reg1;
                        e->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        f->m_parameters.m_reg1 = params.m_reg1;
                        f->m_parameters.m_reg2 = params.m_reg1;
                        f->m_parameters.m_reg3 = 1;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabel:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_offset = 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = params.m_reg2;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                        e->m_parameters.m_reg1 = params.m_reg1;
                        e->m_parameters.m_reg2 = params.m_reg1;
                        e->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        f->m_parameters.m_reg1 = params.m_reg1;
                        f->m_parameters.m_reg2 = params.m_reg1;
                        f->m_parameters.m_reg3 = 1;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelAsOffsetReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_offset = 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        e->m_parameters.m_reg1 = 1;
                        e->m_parameters.m_reg2 = 1;
                        e->m_parameters.m_reg3 = params.m_reg2;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        f->m_parameters.m_reg1 = 1;
                        f->m_parameters.m_reg2 = params.m_reg2;
                        f->m_parameters.m_label = params.m_label;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto g = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                        g->m_parameters.m_reg1 = params.m_reg1;
                        g->m_parameters.m_reg2 = params.m_reg1;
                        g->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                        g->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto h = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        h->m_parameters.m_reg1 = params.m_reg1;
                        h->m_parameters.m_reg2 = params.m_reg1;
                        h->m_parameters.m_reg3 = 1;
                        h->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        b->m_parameters.m_reg1 = params.m_reg1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_label = params.m_label;
                        b->m_parameters.m_offset = params.m_immediate + 1;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        c->m_parameters.m_reg1 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_immediate = params.m_immediate;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_reg2 = params.m_reg2;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_offset = params.m_immediate;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                        e->m_parameters.m_reg1 = params.m_reg1;
                        e->m_parameters.m_reg2 = params.m_reg1;
                        e->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        f->m_parameters.m_reg1 = params.m_reg1;
                        f->m_parameters.m_reg2 = params.m_reg1;
                        f->m_parameters.m_reg3 = 1;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegLabelPlusImmOffsetForReg:
                    {
                        auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        a->m_parameters.m_reg1 = 1;
                        a->m_parameters.m_label = params.m_label;
                        a->m_parameters.m_immediate = params.m_immediate;
                        a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto b = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        b->m_parameters.m_reg1 = 1;
                        b->m_parameters.m_reg2 = 1;
                        b->m_parameters.m_reg3 = params.m_reg2;
                        b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto c = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        c->m_parameters.m_reg1 = params.m_reg1;
                        c->m_parameters.m_reg2 = 1;
                        c->m_parameters.m_label = params.m_label;
                        c->m_parameters.m_offset = params.m_immediate + 1;
                        c->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto d = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                        d->m_parameters.m_reg1 = 1;
                        d->m_parameters.m_label = params.m_label;
                        d->m_parameters.m_immediate = params.m_immediate;
                        d->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto e = static_cast<InstructionToken<Instruction::ADDU>*>(vec.emplace_back(new InstructionToken<Instruction::ADDU>()));
                        e->m_parameters.m_reg1 = 1;
                        e->m_parameters.m_reg2 = 1;
                        e->m_parameters.m_reg3 = params.m_reg2;
                        e->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto f = static_cast<InstructionToken<Instruction::LBU>*>(vec.emplace_back(new InstructionToken<Instruction::LBU>()));
                        f->m_parameters.m_reg1 = 1;
                        f->m_parameters.m_reg2 = params.m_reg2;
                        f->m_parameters.m_offset = params.m_immediate;
                        f->m_parameters.m_label = params.m_label;
                        f->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto g = static_cast<InstructionToken<Instruction::SLL>*>(vec.emplace_back(new InstructionToken<Instruction::SLL>()));
                        g->m_parameters.m_reg1 = params.m_reg1;
                        g->m_parameters.m_reg2 = params.m_reg1;
                        g->m_parameters.m_immediate = 8; //the 8 here refers to the 8 bits of each byte
                        g->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        auto h = static_cast<InstructionToken<Instruction::OR>*>(vec.emplace_back(new InstructionToken<Instruction::OR>()));
                        h->m_parameters.m_reg1 = params.m_reg1;
                        h->m_parameters.m_reg2 = params.m_reg1;
                        h->m_parameters.m_reg3 = 1;
                        h->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                        return vec;
                    }
                    }
                }
            }
        }
    };

    template<>
    class InstructionToken<Instruction::TRUNC_W_D> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the trunc.w.d instruction");
            }
            else
            {
                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::COP1 << 26;
            instruction |= fmt::D << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b001101;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::TRUNC_W_S> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegReg))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the trunc.w.s instruction");
            }
            else
            {

                static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                return vec;
            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::COP1 << 26;
            instruction |= fmt::S << 21;
            instruction |= 0b00000 << 16;
            instruction |= m_parameters.m_reg2 << 11;
            instruction |= m_parameters.m_reg1 << 6;
            instruction |= 0b001101;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::XOR> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegReg | InstructionSyntacticArchetypes::RegRegImm | InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the xor instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    if (!std::in_range<uint16_t>(params.m_immediate))
                    {
                        throw Error::InvalidInstructionException("?", "immediate for or instruction must be in the unsigned 16 bit int range");
                    }
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegRegReg:
                    {
                        static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegRegImm:
                    {
                        static_cast<InstructionToken<Instruction::XORI>*>(vec.emplace_back(new InstructionToken<Instruction::XORI>()))->m_parameters = params;
                        return vec;
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        auto a = static_cast<InstructionToken<Instruction::XORI>*>(vec.emplace_back(new InstructionToken<Instruction::XORI>()));
                        a->m_parameters.m_reg1 = params.m_reg1;
                        a->m_parameters.m_reg2 = params.m_reg1;
                        a->m_parameters.m_immediate = params.m_immediate;
                        return vec;
                    }
                    }
                }

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= cop::SPECIAL << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg3 << 16;
            instruction |= m_parameters.m_reg1 << 11;
            instruction |= 0b100110;
            return instruction;
        }
    };

    template<>
    class InstructionToken<Instruction::XORI> : public InstructionTokenBase
    {
    public:
        static std::vector<TokenBase*> Parse(const std::u32string_view& instructionStr, bool keepPseudoinstructions = false)
        {
            std::vector<TokenBase*> vec;
            InstructionParameters params{};
            if (!parse_instruction(instructionStr, params, InstructionSyntacticArchetypes::RegRegImm | InstructionSyntacticArchetypes::RegImm))
            {
                throw Error::InvalidSyntaxException("?", "Invalid syntax for the xori instruction");
            }
            else
            {
                if (keepPseudoinstructions)
                {
                    static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                    return vec;
                }
                else
                {
                    switch (params.m_archetype)
                    {
                    case InstructionSyntacticArchetypes::RegRegImm:
                    {
                        if (std::in_range<uint16_t>(params.m_immediate))
                        {
                            static_cast<InstructionToken*>(vec.emplace_back(new InstructionToken()))->m_parameters = params;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::XOR>*>(vec.emplace_back(new InstructionToken<Instruction::XOR>()));
                            c->m_parameters.m_reg1 = params.m_reg1;
                            c->m_parameters.m_reg2 = params.m_reg2;
                            c->m_parameters.m_reg3 = 1;
                            return vec;
                        }
                    }
                    case InstructionSyntacticArchetypes::RegImm:
                    {
                        if (std::in_range<uint16_t>(params.m_immediate))
                        {
                            auto a = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = params.m_reg1;
                            a->m_parameters.m_immediate = params.m_immediate;
                            return vec;
                        }
                        else
                        {
                            auto a = static_cast<InstructionToken<Instruction::LUI>*>(vec.emplace_back(new InstructionToken<Instruction::LUI>()));
                            a->m_parameters.m_reg1 = 1;
                            a->m_parameters.m_immediate = params.m_immediate >> 16;
                            a->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto b = static_cast<InstructionToken<Instruction::ORI>*>(vec.emplace_back(new InstructionToken<Instruction::ORI>()));
                            b->m_parameters.m_reg1 = 1;
                            b->m_parameters.m_reg2 = 1;
                            b->m_parameters.m_immediate = params.m_immediate & 0xFFFF;
                            b->m_parameters.m_archetype = InstructionSyntacticArchetypes::CompilerGenerated;
                            auto c = static_cast<InstructionToken<Instruction::XOR>*>(vec.emplace_back(new InstructionToken<Instruction::XOR>()));
                            a->m_parameters.m_reg1 = params.m_reg1;
                            a->m_parameters.m_reg2 = params.m_reg1;
                            c->m_parameters.m_reg3 = 1;
                            return vec;
                        }
                    }
                    }
                }

            }
        }

        uint32_t Encode() override
        {
            uint32_t instruction{ 0U };
            instruction |= 0b001110 << 26;
            instruction |= m_parameters.m_reg2 << 21;
            instruction |= m_parameters.m_reg1 << 16;
            instruction |= m_parameters.m_immediate & 0xFFFF;
            return instruction;
        }
    };

}
