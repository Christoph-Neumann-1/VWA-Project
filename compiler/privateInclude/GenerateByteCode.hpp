#pragma once
#include <ProcessInput.hpp>
namespace vwa
{
    template <typename U>
    // TODO: is there any reason to use is_same here?
    void pushToBc(std::vector<bc::BcToken> &bc, U value) // requires(typeid(T) == typeid(bc::BcToken) || typeid(T) == typeid(uint8_t))
        requires requires(std::vector<bc::BcToken> t)
    {
        t.push_back(bc::BcToken{uint8_t{}});
    }
    {
        uint8_t *data = reinterpret_cast<uint8_t *>(&value);
        for (size_t i = 0; i < sizeof(U); i++)
        {
            bc.push_back(bc::BcToken{data[i]});
        }
    }

    template <typename T>
    void pushToConst(std::vector<uint8_t> &bc, T value)
    {
        uint8_t *data = reinterpret_cast<uint8_t *>(&value);
        for (size_t i = sizeof(T) - 1; i + 1 >= 1; i--)
        {
            bc.push_back(data[i]);
        }
    }

    void compileFunc(Linker::Module &module, Linker &linker, Linker::Symbol &func, std::vector<uint8_t> &constPool, std::vector<bc::BcToken> &bc, Logger &log);
    Linker::Cache::CachedType compileNode(Linker::Module &module, Linker &linker, const Node &node, const Linker::Cache::CachedType fRetT, std::vector<uint8_t> &constPool, std::vector<bc::BcToken> &bc, std::vector<Scope> &scopes, Logger &log);
    // TODO: do I really need to pass in the linker here or would it be better to load dependencies earlier?
    void GenModBc(Linker::Module &mod, Linker &linker, Logger &log);
}