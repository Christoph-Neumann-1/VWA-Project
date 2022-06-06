#include <Linker.hpp>
#include <dlfcn.h>
#include <sys/mman.h>
#include <stdexcept>
#include <unistd.h>

namespace vwa
{

    // This doesn't handle internal functions, but whatever
    void Linker::Cache::generate(Linker &linker)
    {
        structs.clear();
        functions.clear();
        ids.clear();

        ids.insert({{"void"}, Void});
        ids.insert({{"int"}, I64});
        ids.insert({{"float"}, F64});
        ids.insert({{"char"}, U8});
        ids.insert({{"string"}, I64 | (1ul << 32)});
        ids.insert({{"function"}, FPtr});

        for (auto &[_, sym] : linker.symbols)
            if (std::holds_alternative<Symbol::Function>(sym->data))
            {
                functions.push_back({sym});
                if (auto res = ids.insert({sym->name, typeMask | (functions.size() - 1)}).second; !res)
                    throw std::runtime_error("Duplicate function name: " + sym->name.name);
            }
            else
            {
                structs.push_back({sym});
                if (auto res = ids.insert({sym->name, structs.size() - 1 + reservedIndicies}).second; !res)
                    throw std::runtime_error("Duplicate struct name");
            }
        const auto initStruct = [&](CachedStruct &s, auto &self)
        {
            if (s.state == s.Done)
                return;
            if (s.state == s.Processing)
                throw std::runtime_error("Recursive struct definition");
            s.size = 0;
            s.state = s.Processing;
            auto &sym = std::get<Linker::Symbol::Struct>(s.symbol->data);
            s.members.reserve(sym.fields.size());
            for (auto &mem : sym.fields)
            {
                auto memT = typeFromId(mem.type.name);
                if (!~memT)
                    throw std::runtime_error("Unknown type: " + mem.type.name.name);
                if (memT & Cache::typeMask)
                    throw std::runtime_error("Function is not a type");
                CachedStruct::Member m{.type = memT | ((uint64_t{mem.type.pointerDepth} << 32) & pointerDepthMask), .offset = s.size};
                if (memT >= reservedIndicies && !mem.type.pointerDepth)
                    self(structs[memT - reservedIndicies], self);
                s.size += getSizeOfType(m.type);
                s.members.push_back(m);
            }
            s.state = s.Done;
        };
        for (auto &s : structs)
            initStruct(s, initStruct);
        for (auto &f : functions)
        {
            auto &func = std::get<Linker::Symbol::Function>(f.symbol->data);
            auto retT = typeFromId(func.returnType.name);
            // TODO: seperate validate function which throws
            if (!~retT)
                throw std::runtime_error("Unknown type: " + func.returnType.name.name);
            if (retT & Cache::typeMask)
                throw std::runtime_error("Function is not a type");
            retT |= (uint64_t{func.returnType.pointerDepth} << 32) & pointerDepthMask;
            f.returnType = retT;
            f.params.reserve(func.params.size());
            for (auto &param : func.params)
            {
                auto paramT = typeFromId(param.type.name);
                if (!~paramT)
                    throw std::runtime_error("Unknown type: " + param.type.name.name);
                if (paramT & Cache::typeMask)
                    throw std::runtime_error("Function is not a type");
                // TODO: fix for other parts of the program(pointerdepth overriden)
                auto ptrDepth = param.type.pointerDepth + ((paramT & pointerDepthMask) >> 32);
                paramT &= ~pointerDepthMask;
                paramT |= (ptrDepth << 32) & pointerDepthMask;
                f.params.push_back(paramT);
            }
        }
    }

    Linker::Module::DlHandle::~DlHandle()
    {
        if (data)
            dlclose(data);
    }

    const long Linker::Module::Mapping::pageSize = sysconf(_SC_PAGESIZE);

    Linker::Module::Mapping::~Mapping()
    {
        munmap(data - offset, size + offset);
    }

    Linker::Module *Linker::provideModule(Module m)
    {
        auto res = modules.emplace(m.name, std::move(m));
        if (!res.second)
            throw std::runtime_error("Module " + res.first->first + " already exists");
        for (auto &sym : res.first->second.exports)
            symbols.emplace(sym.name, &sym);
        return &res.first->second;
    }

    void Linker::satisfyDependencies()
    {
        // TODO: make sure there are no dependency cycles
        //  Since new modules might be loaded in this function, we create a vector first so we are sure that the order of modules is correct.
        std::vector<Module *> originalMods;
        originalMods.reserve(modules.size());
        for (auto &[_, module] : modules)
            originalMods.push_back(&module);
        for (auto mod : originalMods)
            mod->satisfyDependencies(*this);
    }

    Linker::Symbol &Linker::getSymbol(const Identifier &name)
    {
        auto it = symbols.find(name);
        if (it == symbols.end())
        {
            if (modules.find(name.module) == modules.end())
            {
                loadModule(name.module);
                it = symbols.find(name);
            }
            if (it == symbols.end())
                throw SymbolNotFound(name.name);
        }
        return *it->second;
    }

    Linker::Symbol *Linker::tryFind(const Identifier &name)
    {
        // FIXME: decide whether this should at least attempt loading anythign
        //  auto it = symbols.find(name);
        //  if (it == symbols.end() && modules.find(name.module) == modules.end())
        //  {
        //      loadModule(name.module);
        //      it = symbols.find(name);
        //  }
        auto r = symbols.find(name);
        return r == symbols.end() ? nullptr : r->second;
    }

    Linker::Module &Linker::getModule(const std::string &name)
    {
        auto it = modules.find(name);
        if (it == modules.end())
        {
            loadModule(name);
            it = modules.find(name);
            if (it == modules.end())
                throw std::runtime_error("Module " + name + " not found");
        }
        return it->second;
    }

    Linker::Module *Linker::tryFind(const std::string &name)
    {
        return &modules.find(name)->second;
    }

    void Linker::loadModule(const std::string &name)
    {
        throw std::runtime_error(__func__);
    }

    void Linker::Module::satisfyDependencies(Linker &linker)
    {
        for (auto it = requiredSymbols.begin(); it != requiredSymbols.end();++it)
        {
            auto &sym = std::get<0>(*it);
            auto &newSym = linker.getSymbol(sym.name);
            if (sym != newSym)
                throw std::runtime_error("The definiton of " + sym.name.name + "does not match the one used for compilation");
            *it = &newSym;
        }
    }

    void Linker::Module::patchAddresses(Linker &linker)
    {
        // If ffi dispatch handler

        if(std::holds_alternative<DlHandle>(data))
        {
            return;
            // TODO: someone should really notify the module of this
        }

        auto begin = getData();

        for (auto it = offset; it < getDataSize(); it += bc::getInstructionSize(it[begin].instruction))
        {
            if (begin[it].instruction == bc::CallFunc)
            {
                auto &payload = *reinterpret_cast<uint64_t *>(begin + it + 1);
                auto &sym = *std::get<1>(requiredSymbols[payload]);
                auto &f = std::get<0>(sym.data);
                switch (f.type)
                {
                case Symbol::Function::Compiled:
                {
                    f.type = f.Internal;
                    f.bcAddress = f.idx + linker.modules[sym.name.module].getData() + linker.modules[sym.name.module].offset;
                    // There need not be any checks here for internal functions, as that would be done earlier if I so desired
                    // Though it would probably help to move this conditional somewhere else
                    [[fallthrough]];
                }
                case Symbol::Function::Internal:
                {
                    begin[it] = {bc::JumpFuncAbs};
                    payload = reinterpret_cast<uint64_t>(f.bcAddress);
                    break;
                }
                case Symbol::Function::External:
                {
                    it[begin] = {bc::JumpFFI};
                    payload = reinterpret_cast<uint64_t>(f.ffi);
                    break;
                }
                default:
                    throw std::runtime_error("Undefined function");
                    // FIXME: before this step, all indices must somehow be resolved to actual adresses.}
                }
            }
        }
    }

    void Linker::patchAddresses()
    {
        // TODO what happens when realizing the implementation is unavailable?
        // Should satisfyDepependencies by called beforehand?
        for (auto &m : modules)
            m.second.patchAddresses(*this);
    }
};