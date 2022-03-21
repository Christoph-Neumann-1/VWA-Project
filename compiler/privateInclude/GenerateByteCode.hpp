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

    // TODO: I really shouldn't have so many structs with the same members.
    struct NodeResult
    {
        size_t type = PrimitiveTypes::Void;
        uint32_t pointerDepth = 0;
    };

    // TODO: push to const
    void
    compileFunc(Linker::Module *module, Cache *cache, const Pass1Result::Function &func, std::vector<uint8_t> &constPool, std::vector<bc::BcToken> &bc, Logger &log);
    NodeResult compileNode(Linker::Module *module, Cache *cache, const Node *node, const NodeResult fRetT, std::vector<uint8_t> &constPool, std::vector<bc::BcToken> &bc, std::vector<Scope> &scopes, Logger &log);
    inline void GenModBc(Linker::Module *mod, const Pass1Result &pass1, Cache *cache, Logger &log)
    {
        log << Logger::Info << "Generating bytecode\n";
        // This is used to represent the constant pool. Its relative adress is always known, since it's at the beginning of the module.
        // The pool grows in the opposite direction of the bytecode so as to not introduce additional complexity.
        // It is not merged with bytecode because that would lead to loads of copying data around. Putting the data in the middle of the code is possible, but not easy to work with,
        // this method also makes reuse possible by searching for overlapping data.
        // Data must be pushed in reverse order
        std::vector<uint8_t> constants{};
        std::vector<bc::BcToken> bc;
        for (auto &func : pass1.functions)
            compileFunc(mod, cache, func, constants, bc, log);

        auto bcSize = bc.size();
        bc.reserve(bc.size() + constants.size());
        if (!bcSize)
        {
            log << Logger::Warning << "No code in module\n";
            return;
        }
        for (auto i = bcSize - 1; i + 1 > constants.size(); --i)
        {
            bc[i] = bc[i - constants.size()];
        }
        {
            int i = constants.size() - 1, j = 0;
            while (i + 1 > 0)
            {
                bc[j++] = {constants[i--]};
            }
        }
        mod->data = std::move(bc);
    }
}