#pragma once
#include <string>
#include <unicode/uchar.h>
namespace NeoMIPS
{
    namespace StringUtil
    {
        inline bool is_space(char32_t c)
        {
            return std::isspace(c);
        }

        inline bool is_separator(char32_t c)
        {
            return u_isblank(c) || u_ispunct(c);
        }
    }

}
