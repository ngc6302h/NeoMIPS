// NeoMIPS.cpp : Este archivo contiene la función "main". La ejecución del programa comienza y termina ahí.
//
#include <iostream>
#include "argumentprocessor.hpp"
#include "util.hpp"
#include "executioncontext.hpp"
#include "types.hpp"
#include "util.hpp"

using namespace NeoMIPS;

int main(int argc, char** argv)
{
    std::cout << "------------      NeoMIPS    --------------\n";
    std::cout << "       New Educational Open-Source\n";
    std::cout << "     MIPS32 Assembler and Interpreter\n";
    std::cout << "------------ by charlesdeepk --------------\n";

    argmap_t options;
    ArgumentProcessor::ReadArguments(argc, argv, options);

    ExecutionContext context(options);
    context.Run();
    
}

