#include <Linker.hpp>
#include <dlfcn.h>
#include <sys/mman.h>
#include <stdexcept>
#include <unistd.h>
#include <sstream>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <array>
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
        ids.insert({{"string"}, I64 | (1ul << 32)}); // FIXME: this should be replaced by I64 ptr when found, not later
        ids.insert({{"function"}, FPtr});            // TODO: better way to encode this

        const auto processSym = [&](Symbol *sym)
        {
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
        };

        for (auto &[_, sym] : linker.symbols)
            processSym(sym);

        // const auto requireSymbol = [&](Identifier sym) -> void
        // {
        //     if (ids.contains(sym))
        //         return;
        //     decltype(auto) symbol = linker.getSymbol(sym);
        //     processSym(&symbol);
        // };

        const auto requireType = [&](Identifier type) -> CachedType
        {
            // requireSymbol(type);
            decltype(auto) t = typeFromId(type);
            if (~t && !(t & typeMask))
                return t;
            if (t & typeMask)
                throw std::runtime_error("Not a type");
            if (!~t)
                throw std::runtime_error("Invalid type");
            throw std::runtime_error("just no");
        };

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
                auto memT = requireType(mem.type.name);
                CachedStruct::Member m{.type = memT | ((uint64_t{mem.type.pointerDepth} << 32) & pointerDepthMask), .offset = s.size};
                if (memT >= reservedIndicies && !mem.type.pointerDepth)
                    self(structs[memT - reservedIndicies], self);
                s.size += getSizeOfType(m.type);
                s.members.push_back(m);
            }
            s.state = s.Done;
        };
        for (size_t i{}; i < structs.size(); i++)
            initStruct(structs[i], initStruct);
        for (auto &f : functions)
        {
            auto &func = std::get<Linker::Symbol::Function>(f.symbol->data);
            auto retT = requireType(func.returnType.name);
            retT |= (uint64_t{func.returnType.pointerDepth} << 32) & pointerDepthMask;
            f.returnType = retT;
            f.params.reserve(func.params.size());
            for (auto &param : func.params)
            {
                auto paramT = requireType(param.type.name);
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
        // throw std::runtime_error(__func__);
        // FIXME I should really cache this
        // FIXME: allow loading uncompiled stuff
        // FIXME: prefer: native > bc > interface
        std::array<std::unordered_set<std::filesystem::path>, 3> candidates;
        for (auto &dir : searchPaths)
            for (auto &file : std::filesystem::recursive_directory_iterator(dir))
                if (!file.is_regular_file())
                    continue;
                else if (file.path().filename() == name + ".interface")
                    candidates[2].emplace(std::filesystem::absolute(file.path()));
                else if (file.path().filename() == name + ".bc")
                    candidates[1].emplace(std::filesystem::absolute(file.path()));
                else if (file.path().filename() == name + ".native")
                    candidates[0].emplace(std::filesystem::absolute(file.path()));

        auto resolve = [](const std::unordered_set<std::filesystem::path> &results) -> const std::filesystem::path &
        {
            const std::filesystem::path *current{};
            for (auto &r : results)
                if (!current || std::filesystem::last_write_time(r) > std::filesystem::last_write_time(*current))
                    current = &r;
            return *current;
        };

        while (1)
        {
            auto category = candidates[0].size() ? 0 : candidates[1].size() ? 1
                                                   : candidates[2].size()   ? 2
                                                                            : ({ throw std::runtime_error("No candidate file for module " + name);-1; });

            auto file = resolve(candidates[category]);

            switch (category)
            {
            case 0:
            {
                auto fname = file.string();
                Module::DlHandle handle{dlopen(fname.c_str(), RTLD_LAZY)};
                if (!handle)
                {
                    puts(dlerror());
                    candidates[category].erase(file);
                    continue;
                }
                auto loadFcn = reinterpret_cast<Linker::Module (*)()>(dlsym(handle, "VM_ONLOAD"));
                if (!loadFcn)
                {
                    candidates[category].erase(file);
                    continue;
                }
                auto module = loadFcn();
                module.data.emplace<Linker::Module::DlHandle>(std::move(handle));
                provideModule(std::move(module));
                return;
            }
            case 1:
            case 2:
            {
                std::ifstream t(file);
                t.seekg(0, std::ios::end);
                size_t size = t.tellg();
                t.seekg(0);
                std::unique_ptr<char[]> buf(new char[size]);
                t.read(buf.get(), size);
                Module *mod;
                try
                {
                    mod = provideModule(deserialize(std::string_view(buf.get(), size)));
                }
                catch (std::runtime_error &e)
                {
                    candidates[category].erase(file);
                    continue;
                }
                for (auto &n : mod->imports)
                    getModule(n);
                mod->imports.clear();
                return;
            }
            default:
                throw std::runtime_error(".");
            }
        }
    }

    void Linker::Module::satisfyDependencies(Linker &linker)
    {
        for (auto it = requiredSymbols.begin(); it != requiredSymbols.end(); ++it)
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

        if (std::holds_alternative<DlHandle>(data))
        {
            return;
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
                    // This seems pretty inefficient
                    f.bcAddress = f.idx + linker.modules[sym.name.module].getData() + linker.modules[sym.name.module].offset;
                    // This abomination needs to be corrected.
                    //  There need not be any checks here for internal functions, as that would be done earlier if I so desired
                    //  Though it would probably help to move this conditional somewhere else
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
        for (auto &sym : exports)
            if (auto it = std::get_if<Symbol::Function>(&sym.data); it)
            {
                switch (it->type)
                {
                case Symbol::Function::Unlinked:
                    throw std::runtime_error("Missing function definition for " + sym.name.name);
                case Symbol::Function::Compiled:
                    it->bcAddress = getData() + offset + it->idx;
                    it->type = Symbol::Function::Internal;
                case Symbol::Function::Internal:
                    // Shouldn't be possible, but why not. If it works then it works
                    break;
                case Symbol::Function::External:
                    throw std::runtime_error("broken module");
                }
            }
    }

    void Linker::patchAddresses()
    {
        // TODO what happens when realizing the implementation is unavailable?
        // Should satisfyDepependencies by called beforehand?
        for (auto &m : modules)
            m.second.patchAddresses(*this);
        for (auto &m : modules)
            if (auto it = std::get_if<Module::DlHandle>(&m.second.data))
            {
                auto f = reinterpret_cast<void (*)(Linker &)>(dlsym(*it, "VM_ONLINK"));
                if (f)
                    f(*this);
            }
    }
    // TODO: reuse most of this for the boilerplate generator
    std::string Linker::serialize(const Module &mod, bool interface)
    {
        // TODO: hashes to go faster

        // TODO: add concepts to constrain input
        auto emplaceLength = [](auto &stream, auto length)
        {
            for (size_t i = 0; i < sizeof(length); i++)
                stream << i[(char *)&length];
        };
        auto emplaceString = [&emplaceLength](auto &stream, const std::string &str)
        {
            emplaceLength(stream, str.length());
            stream << str;
        };
        // TODO: no qualified name for builtins
        auto emplaceQualified = [&emplaceString](auto &stream, const Identifier &id)
        {
            emplaceString(stream, id.name);
            emplaceString(stream, id.module);
        };
        auto emplaceType = [&emplaceQualified, &emplaceLength](auto &stream, const VarType &t)
        {
            emplaceQualified(stream, t.name);
            emplaceLength(stream, t.pointerDepth);
        };
        auto dumpBin = [](auto &stream, const auto *data, const size_t size)
        {
            for (size_t i = 0; i < size; i++)
                stream << i[(char *)data];
        };

        std::stringstream output;
        output << "VWA_MODULE";
        // auto n_l = mod.name.length();
        // char *n_lp = static_cast<char *>(static_cast<void *>(&n_l));
        // output << 0 [n_lp] << 1 [n_lp] << 2 [n_lp] << 3 [n_lp] << 4 [n_lp] << 5 [n_lp] << 6 [n_lp] << 7 [n_lp] << '\n';
        // emplaceLength(output, mod.name.length());
        // output << mod.name << '\n';
        emplaceString(output, mod.name);
        if (mod.exports.size())
        {
            output << 'E';
            emplaceLength(output, mod.exports.size()); // To allow preallocation of vector
            for (auto &e : mod.exports)
            {
                bool isStruct = std::holds_alternative<Symbol::Struct>(e.data);
                output << (isStruct ? "S" : "F");
                emplaceString(output, e.name.name);
                if (isStruct)
                {
                    auto &s = std::get<Symbol::Struct>(e.data);
                    emplaceLength(output, s.fields.size());
                    for (auto &f : s.fields)
                    {
                        emplaceString(output, f.name);
                        emplaceType(output, f.type);
                        // FIXME: make sure I'm dealing with implicit module names properly e.g foo instead of module::foo
                    }
                }
                else
                {
                    auto &f = std::get<Symbol::Function>(e.data);
                    emplaceType(output, f.returnType);
                    emplaceLength(output, f.params.size());
                    for (auto &p : f.params)
                        emplaceType(output, p.type);
                    emplaceLength(output, f.idx);
                }
            }
        }

        if (interface && mod.imports.size())
        {
            output << 'M';
            emplaceLength(output, mod.imports.size());
            for (auto &i : mod.imports)
                emplaceString(output, i);
        }
        else if (mod.requiredSymbols.size())
        {
            output << 'I';
            emplaceLength(output, mod.requiredSymbols.size()); // To allow preallocation of vector
            for (auto &e : mod.requiredSymbols)
            {
                auto &sym = std::get<Symbol>(e);
                bool isStruct = std::holds_alternative<Symbol::Struct>(sym.data);
                output << (isStruct ? "S" : "F");
                emplaceQualified(output, sym.name);
                if (isStruct)
                {
                    auto &s = std::get<Symbol::Struct>(sym.data);
                    emplaceLength(output, s.fields.size());
                    for (auto &f : s.fields)
                    {
                        emplaceString(output, f.name);
                        emplaceType(output, f.type);
                        // FIXME: make sure I'm dealing with implicit module names properly e.g foo instead of module::foo
                    }
                }
                else
                {
                    auto &f = std::get<Symbol::Function>(sym.data);
                    emplaceType(output, f.returnType);
                    emplaceLength(output, f.params.size());
                    for (auto &p : f.params)
                        emplaceType(output, p.type);
                }
            }
        }
        if (!interface && mod.getDataSize())
        {
            output << 'C';
            emplaceLength(output, mod.offset);
            emplaceLength(output, mod.getDataSize());
            // auto d_l = mod.getDataSize();
            // auto dlp = (char *)&d_l;
            // output << 0 [dlp] << 1 [dlp] << 2 [dlp] << 3 [dlp] << 4 [dlp] << 5 [dlp] << 6 [dlp] << 7 [dlp] << '\n';
            // auto dp = mod.getData();
            // for (size_t i = 0; i < d_l; i++)
            //     output << (dp++)->value;
            dumpBin(output, mod.getData(), mod.getDataSize());
        }
        return output.str();
    }

    Linker::Module Linker::deserialize(std::string_view in)
    {
        size_t pos{};
        auto requireLength = [&pos, in](size_t l)
        {if(pos+l>in.length())throw std::runtime_error("deserialization failed, unexpected eof") ; };
        auto readSize = [&pos, in, &requireLength]() -> uint64_t
        {
            requireLength(sizeof(uint64_t));
            // uint64_t r;
            // for (size_t i = 0; i < sizeof(uint64_t); i++)
            //     i[(char *)&r] = in[pos++];
            uint64_t r = *(const uint64_t *)&in[pos];
            pos += sizeof(uint64_t);
            return r;
            // I should just cast to int pointer here or sthg
        };
        auto readSize32 = [&pos, in, &requireLength]() -> uint32_t
        {
            requireLength(sizeof(uint32_t));
            // uint32_t r;
            // for (size_t i = 0; i < sizeof(uint32_t); i++)
            //     i[(char *)&r] = in[pos++];
            // return r;
            uint32_t r = *(const uint32_t *)&in[pos];
            pos += sizeof(uint32_t);
            return r;
        };
        auto readStr = [&pos, in, &readSize, &requireLength]() -> std::string_view
        {
            auto l = readSize();
            requireLength(l);
            auto s = in.substr(pos, l);
            pos += l;
            return s;
        };
        auto readQualified = [&readStr]() -> Identifier
        {
            return {std::string{readStr()}, std::string{readStr()}};
        };
        auto readType = [&readQualified, &readSize32]() -> VarType
        {
            return {readQualified(), readSize32()};
        };

        requireLength(sizeof("VWA_MODULE") - 1);
        if (in.substr(pos, sizeof("VWA_MODULE") - 1) != "VWA_MODULE")
            throw std::runtime_error("Missing VWA_MODULE. Not valid");
        pos += sizeof("VWA_MODULE") - 1;
        Module ret;
        ret.name = readStr();
        bool hadExports{0}, hadImports{0}, hadCode{0}, hadInterfaceImports{0};
        // Ok, so make this handle unordered sections and maybe no sections at all if some moron compiles an empty module
        for (int i = 0; i < 3; i++)
        {
            if (pos == in.length())
                return ret;
            switch (in[pos++])
            {
            case 'E':
            {
                if (hadExports)
                    throw std::runtime_error("Duplicate section");
                hadExports = 1;
                auto l = readSize();
                ret.exports.reserve(l);
                while (l--)
                {
                    requireLength(1);
                    auto t = in[pos++];
                    auto name = readStr();
                    switch (t)
                    {
                    case 'F':
                    {
                        ret.exports.push_back(Symbol{{std::string{name}, ret.name}, Symbol::Function{}});
                        auto &f = std::get<Symbol::Function>(ret.exports.back().data);
                        f.returnType = readType();
                        auto pl = readSize();
                        f.params.reserve(pl);
                        for (; pl--;)
                            f.params.push_back({.type = readType()});
                        f.idx = readSize();
                        f.type = Symbol::Function::Compiled;
                        break;
                    }
                    case 'S':
                    {
                        ret.exports.push_back(Symbol{{std::string{name}, ret.name}, {Symbol::Struct{}}});
                        auto &s = std::get<Symbol::Struct>(ret.exports.back().data);
                        auto fl = readSize();
                        s.fields.reserve(fl);
                        for (; fl--;)
                            s.fields.push_back({std::string{readStr()}, readType()});
                        break;
                    }
                    default:
                        throw std::runtime_error("Expected Function or struct");
                    }
                }
                break;
            }
            case 'I':
            {
                if (hadImports)
                    throw std::runtime_error("Duplicate section");
                if (hadInterfaceImports)
                    throw std::runtime_error("Cannot have interface imports alongside normal ones");
                hadImports = 1;
                auto l = readSize();
                ret.requiredSymbols.reserve(l);
                while (l--)
                {
                    requireLength(1);
                    auto t = in[pos++];
                    auto name = readQualified();
                    switch (t)
                    {
                    case 'F':
                    {
                        ret.requiredSymbols.push_back(Symbol{std::move(name), Symbol::Function{}});
                        auto &f = std::get<Symbol::Function>(std::get<Symbol>(ret.requiredSymbols.back()).data);
                        f.returnType = readType();
                        auto pl = readSize();
                        f.params.reserve(pl);
                        for (; pl--;)
                            f.params.push_back({.type = readType()});
                        f.type = Symbol::Function::Unlinked;
                        break;
                    }
                    case 'S':
                    {
                        ret.requiredSymbols.push_back(Symbol{std::move(name), {Symbol::Struct{}}});
                        auto &s = std::get<Symbol::Struct>(std::get<Symbol>(ret.requiredSymbols.back()).data);
                        auto fl = readSize();
                        s.fields.reserve(fl);
                        for (; fl--;)
                            s.fields.push_back({std::string{readStr()}, readType()});
                        break;
                    }
                    default:
                        throw std::runtime_error("Expected Function or struct");
                    }
                }
                break;
            }
            case 'C':
            {
                // TODO: save the offset? Done
                if (hadCode)
                    // TODO: implemente memory mapping for code
                    throw std::runtime_error("Duplicate section");
                if (hadInterfaceImports)
                    throw std::runtime_error("Cannot have code in interface file");
                hadCode = 1;
                ret.offset = readSize();
                auto data = readStr();
                std::vector<bc::BcToken> d;
                d.resize(data.size());
                std::memcpy(d.data(), data.data(), data.size());
                ret.data = std::move(d);
                break;
            }
            case 'M':
            {
                if (hadInterfaceImports)
                    throw std::runtime_error("Duplicate section");
                if (hadCode)
                    throw std::runtime_error("Cannot have code in interface file");
                if (hadImports)
                    throw std::runtime_error("Cannot have interface imports alongside normal ones");

                auto l = readSize();
                ret.imports.reserve(l);
                while (l--)
                    ret.imports.emplace_back(readStr());
                break;
            }
            default:
                throw std::runtime_error("Invalid section header");
            }
        }
        if (pos != in.length())
            throw std::runtime_error("Expected end of file");
        // return *provideModule(std::move(ret));
        return ret;
    }
};