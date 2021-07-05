#include <string>
#include <filesystem>
#include <fstream>
#include <istream>
#include <codecvt>
#include <locale>
#include "error.hpp"
#include "filereader.hpp"

std::u32string* NeoMIPS::FileReader::ReadWithEncoding(const std::string& path)
{
    if (!std::filesystem::exists(path))
    {
        throw Error::FileNotFoundException("", std::string("File \"").append(path).append("\" does not exist."));
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.good())
    {
        throw Error::FileReadException("", std::string("Could not read input file!"));
    }

    file.imbue(std::locale("en_US.UTF8"));
    std::u8string sourceEncodingStr{};
    std::u32string* destEncodingStr = new std::u32string();

    sourceEncodingStr.resize(std::filesystem::file_size(path));
    destEncodingStr->resize(std::filesystem::file_size(path));

    file.read((char*)sourceEncodingStr.data(), std::filesystem::file_size(path));

    auto& facet = std::use_facet<std::codecvt<char32_t, char8_t, std::mbstate_t>>(std::locale("en_US.UTF8"));
    std::mbstate_t state{};
    const char8_t* from_next{};
    char32_t* to_next;
    if (facet.in(state, sourceEncodingStr.data(), &sourceEncodingStr.data()[sourceEncodingStr.size()], from_next, destEncodingStr->data(), &destEncodingStr->data()[destEncodingStr->size() - 1], to_next) != std::codecvt_base::ok)
    {
        throw Error::EncodingTranslationException("", std::string("Failed to convert input from UTF-8 to UTF-32"));
    }

    return destEncodingStr;
}