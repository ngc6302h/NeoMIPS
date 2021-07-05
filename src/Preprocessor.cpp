#include "Preprocessor.hpp"
#include "include/ctre-unicode.hpp"
#include "PreprocessorRegex.hpp"
#include "filereader.hpp"
#include "lexer_util.hpp"
#include <unicode/uchar.h>
#include <boost/regex.hpp>
namespace NeoMIPS
{


        void Preprocessor::process_include(std::u32string& input, const auto& toReplaceStart, const auto& toReplaceEnd, const std::u32string& replaceWith)
        {
            input.replace(toReplaceStart, toReplaceEnd, *FileReader::ReadWithEncoding(to_ascii_string(replaceWith)));
        }

        void Preprocessor::process_eqv(std::u32string& input, const auto& toRemoveStart, const auto& toRemoveEnd, const std::u32string& define, const std::u32string& replaceWith)
        {
            input.erase(toRemoveStart, toRemoveEnd);
            std::size_t pos = 0;
            while ((pos = input.find(define, pos+1)) != std::u32string::npos)
            {
                //check we're not in a string
                uint32_t hits = 0;
                for (size_t i = 0; i < pos; ++i)
                {
                    if (input[i] == U'\'')
                    {
                        if (input[i - 1] != U'\\') ++hits;
                    }
                }
                if (hits % 2 != 0) continue;

                
                if (is_separator(input[pos-1]) && is_separator(input[pos + std::distance(toRemoveStart, toRemoveEnd)]))
                {
                    input.replace(pos, std::distance(toRemoveStart, toRemoveEnd), replaceWith);
                }
            }
        }

        void process_macro(std::u32string& input, const auto& toRemoveStart, const auto& toRemoveEnd, const std::u32string& macroName, const std::u32string& macroArgs, const std::u32string macroBody)
        {
            input.erase(toRemoveStart, toRemoveEnd);

            std::size_t pos = 0;
            while ((pos = input.find(macroName, pos+1)) != std::u32string::npos)
            {
                //check we're not in a string
                uint32_t hits = 0;
                for (size_t i = 0; i < pos; ++i)
                {
                    if (input[i] == U'\'')
                    {
                        if (input[i - 1] != U'\\') ++hits;
                    }
                }
                if (hits % 2 != 0) continue;


                if (is_separator(input[pos-1]) && is_separator(input[pos + std::distance(toRemoveStart, toRemoveEnd)]))
                {

                }
            }
        }

        void Preprocessor::Preprocess(std::u32string& input)
        {
            std::vector<std::tuple<std::u32string, std::u32string>> macros{};

            for (auto& match : ctre::multiline_range<Regex::includeMacroAndEqvPattern>(input))
            {
                std::u32string macroType{ match.get<1>() };
                if (macroType.compare(U".include") == 0)
                {
                    process_include(input, match.get<0>().begin(), match.get<0>().end(), match.get<9>().to_string());
                }
                else if (macroType.compare(U".eqv") == 0)
                {
                    process_eqv(input, match.get<0>().begin(), match.get<0>().end(), match.get<6>().to_string(), match.get<7>().to_string());
                }
                else
                {
                    process_macro(input, match.get<0>().begin(), match.get<0>().end(), match.get<2>().to_string(), match.get<3>().to_string(), match.get<4>().to_string());
                }
            }
        }

}
