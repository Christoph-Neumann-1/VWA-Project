#pragma once
#include <Parser.hpp>
#include <Bytecode.hpp>
#include <Linker.hpp>
#include <Log.hpp>

namespace vwa
{
    std::vector<Linker::Module *> compile(std::vector<Linker::Module> pass1, Linker &linker, Logger &log);
}
