#pragma once

#include <string>
namespace NeoMIPS
{
    class Preprocessor
    {
    public:
        static void Preprocess(std::u32string& input);
        static void process_include(std::u32string& input, const auto& toReplaceStart, const auto& toReplaceEnd, const std::u32string& replaceWith);
        static void process_eqv(std::u32string& input, const auto& toRemoveStart, const auto& toRemoveEnd, const std::u32string& define, const std::u32string& replaceWith);
        static void process_macro(std::u32string& input, const auto& toRemoveStart, const auto& toRemoveEnd, const std::u32string& macroName, const std::u32string& macroArgs, const std::u32string macroBody);
    };
}
