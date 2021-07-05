#pragma once
#include <string>
//#include "filereader.hpp"

namespace NeoMIPS
{
    class FileReader
    {
    public:
        static std::u32string* ReadWithEncoding(const std::string& path);
    };


}
