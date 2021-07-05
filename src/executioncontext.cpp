#include "executioncontext.hpp"
#include "argumentprocessor.hpp"
#include "lexer.hpp"
#include "filereader.hpp"

namespace NeoMIPS
{
    void ExecutionContext::Run()
    {
        try
        {
            std::unique_ptr<std::u32string> code(FileReader::ReadWithEncoding(GetSourcePath()));
            Lexer lexer(m_options);
            lexer.Tokenize(*code.get());
        }
        catch (Error::NeoMIPSException e)
        {

        }
    }

}