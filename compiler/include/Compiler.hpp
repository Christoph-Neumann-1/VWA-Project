#pragma once
#include <Parser.hpp>
#include <Bytecode.hpp>
#include <Linker.hpp>
namespace vwa
{
    std::vector<const Linker::Module *> compile(std::vector<std::pair<std::string, Pass1Result>> pass1, Linker &linker);
}
