#pragma once
#include "include/ctre-unicode.hpp"

namespace NeoMIPS
{
    namespace Regex
    {
        constexpr auto includeMacroAndEqvPattern = ctll::fixed_string{
            U"(?:(?:(?:(\\.macro)\\s+(\\S+)\\s*(\\(.*\\))?\\s*\\n([\\s\\S]+?)\\n\\s*\\.end_macro)|(?:(\\.eqv)(\\s\\S+\\b)(.*))|(?:(\\.include)(\\s\\S+\\b))))"
        };
    }
}
