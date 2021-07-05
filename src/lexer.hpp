#pragma once
#include "token.hpp"
#include "mips32isa.hpp"

namespace NeoMIPS
{
    class Lexer
    {
    private:
        const argmap_t& m_options;
        std::unique_ptr<std::vector<TokenBase*>> m_tokens;
        uint32_t m_index;

        void ResolveEQV(std::u32string& source);
        void GetMacroDeclaratios(std::u32string& source, std::vector<MacroDeclaration>& macros);
        void ResolveMacros(std::u32string& source);
        void ExpandMacros(std::vector<MacroDeclaration>& macros, std::u32string& source);
        void ParseTag(const std::u32string& source);
        void ParseAlphabetCharacters(const std::u32string& source);
        void ParseDirective(const std::u32string& source);
        void ParseDirectiveStatement(const std::u32string& source, ISA::Directive directive);
        void ParseInstructionStatement(const std::u32string& source, ISA::Instruction instruction);
        void split_line(const std::u32string& source, std::vector<std::u32string>& strs);
        void skip_comment(const std::u32string& source);
        void InitialState(const std::u32string& source);

    public:
        Lexer(const argmap_t& options) : m_options(options), m_tokens(new std::vector<TokenBase*>()), m_index(0) {};
        
        std::unique_ptr<std::vector<TokenBase*>> Tokenize(std::u32string& source);

        ~Lexer()
        {
            for (TokenBase* tb : *m_tokens)
            {
                delete tb;
            }
        }
        
    };
}