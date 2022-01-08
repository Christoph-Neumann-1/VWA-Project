#include <Linker.hpp>
#include <dlfcn.h>
#include <stdexcept>

namespace vwa
{
    Linker::Module::~Module()
    {
        if (std::holds_alternative<void *>(data))
        {
            dlclose(std::get<void *>(data));
        }
    }

    Linker::Module &Linker::provideModule(std::string name, Module module)
    {
        auto res = modules.emplace(std::move(name), std::move(module));
        if (!res.second)
        {
            throw std::runtime_error("Module " + name + " already exists");
        }
        return res.first->second;
    }

    const Linker::Module &Linker::requireModule(std::string name)
    {
        auto it = modules.find(name);
        if (it == modules.end())
        {
            throw std::runtime_error("Dynamic module loading is not implemented yet");
        }
        return it->second;
    }
    void Linker::satisfyDependencies()
    {
        // Since new modules might be loaded in this function, we create a vector first so we are sure that the order of modules is correct.
        std::vector<Module *> originalMods;
        for (auto &[_, module] : modules)
        {
            originalMods.push_back(&module);
        }
        for (auto mod : originalMods)
            mod->satisfyDependencies(*this);
    }

    void Linker::Module::satisfyDependencies(Linker &linker)
    {
        for (auto &import : importedModules)
        {
            auto module = linker.requireModule(import);
            for (auto &sym : module.exportedSymbols)
            {
                auto res = symbols.emplace(sym.name, &sym);
                if (!res.second)
                {
                    throw std::runtime_error("Symbol " + sym.name + " already exists. Attempted import from " + import);
                }
            }
        }
        for (auto &f : requiredSymbols)
        {
            auto &sym = std::get<Symbol>(f);
            auto it = symbols.find(sym.name);
            if (it == symbols.end())
            {
                throw std::runtime_error("Symbol " + sym.name + " is required");
            }
            if (sym.data.index() != it->second->data.index())
            {
                throw std::runtime_error("Error could not satisfy dependency " + sym.name + ". Type mismatch");
            }
            f = it->second;
        }
    }

    void Linker::patchAddresses()
    {
        // TODO
        throw std::runtime_error("Not implemented");
    }
};