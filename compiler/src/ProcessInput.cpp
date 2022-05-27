#include <ProcessInput.hpp>
#include <GenerateByteCode.hpp>

// TODO: replace all types with ids and remember to save them in cached structs
// TODO: remove copy constructors
//FIXME: I have somehow broken structs
namespace vwa
{
    // size_t typeFromId(const Identifier &id, const std::vector<CachedStruct> &structs)
    // {
    //     // I really hope no one is stupid enough to overwrite one of the primitive types
    //     if (id.module_.empty())
    //     {
    //         if (id.name == "void")
    //             return Void;
    //         if (id.name == "int")
    //             return I64;
    //         if (id.name == "float")
    //             return F64;
    //         if (id.name == "char")
    //             return U8;
    //     }

    //     auto it = std::find_if(structs.begin(), structs.end(), [&id](const CachedStruct &s)
    //                            { return s.symbol->name == id; });
    //     if (it == structs.end())
    //         throw std::runtime_error("Could not find struct: " + id.name + " in module: " + id.module_);
    //     return Linker::Cache::reservedIndices + std::distance(structs.begin(), it);
    // }
    // size_t getSizeOfType(size_t t, size_t ptr, const std::vector<CachedStruct> &structs)
    // {
    //     if (ptr)
    //         return 8;
    //     switch (t)
    //     {
    //     case Void:
    //         return 0;
    //     case F64:
    //     case I64:
    //         return 8;
    //     case U8:
    //         return 1;
    //     default:
    //         return structs[t - Linker::Cache::reservedIndices].size;
    //     }
    // }

    // Returns the size of the struct in bytes, if it has not been calculated it is stored.
    // // If a cycle is detected an exception is thrown
    // const CachedStruct &getStructInfo(CachedStruct &struc, std::vector<CachedStruct> &structs)
    // {
    //     if (struc.state == CachedStruct::Processing)
    //         throw std::runtime_error("Error: cycle detected."); // TODO: proper backtrace
    //     if (struc.state == CachedStruct::Finished)
    //         return struc;
    //     struc.size = 0;
    //     struc.state = CachedStruct::Processing;
    //     for (auto &member : std::get<Linker::Symbol::Struct>(struc.symbol->data).fields)
    //     {
    //         auto t = typeFromId(member.type, structs);
    //         struc.members.push_back({struc.size, t, member.pointerDepth});
    //         if (t < Linker::Cache::reservedIndices)
    //         {
    //             // We can't use this function for structs,since it is unknown whether their size has been calculated yet
    //             struc.size += getSizeOfType(t, 0, structs);
    //             continue;
    //         }
    //         auto &mem = getStructInfo(structs[t - Linker::Cache::reservedIndices], structs);
    //         struc.size += mem.size;
    //     }
    //     struc.state = CachedStruct::Finished;
    //     return struc;
    // }

    // void finishFunctionCache(CachedFunction &func, const std::vector<CachedStruct> &structs)
    // {
    //     auto &f = std::get<Linker::Symbol::Function>(func.symbol->data);
    //     func.returnType = {typeFromId(f.returnType.type, structs), f.returnType.pointerDepth};
    //     for (auto &param : f.params)
    //     {
    //         func.args.push_back({typeFromId(param.type, structs), param.pointerDepth});
    //     }
    // }

    // // FIXME: only insert module name into Identifier when saving to Module, otherwise just accept, that some do not have a fully qualified name

    // Cache generateCache(ProcessingResult &result, Logger &log)
    // {
    //     // TODO: I need to make this more efficient, linear search through the entire list of structs is horrible.
    //     Cache cache;
    //     cache.map.insert({{"void"}, {Void, Cache::Type::Struct}});
    //     cache.map.insert({{"int"}, {I64, Cache::Type::Struct}});
    //     cache.map.insert({{"float"}, {F64, Cache::Type::Struct}});
    //     cache.map.insert({{"char"}, {U8, Cache::Type::Struct}});

    //     for (auto &sym : result.module->symbols)
    //         if (std::holds_alternative<Linker::Module::Symbol::Function>(sym.second->data))
    //         {
    //             cache.functions.push_back(CachedFunction{.symbol = sym.second, .body = ({
    //                                                                                auto res = result.FunctionBodies.find(sym.first.name);
    //                                                                                res == result.FunctionBodies.end() ? nullptr : res->second;
    //                                                                            }),
    //                                                      .internal = false});
    //             if (auto res = cache.map.insert({{sym.first}, {cache.functions.size() - 1, cache.Function}}).second; !res)
    //             {
    //                 log << Logger::Error << "Duplicate symbol: " << sym.first.name << '\n';
    //                 throw std::runtime_error("Duplicate symbol");
    //             }
    //         }
    //         else
    //         {
    //             cache.structs.push_back(CachedStruct{.symbol = sym.second, .internal = false});
    //             if (auto res = cache.map.insert({{sym.first}, {cache.structs.size() - 1 + Linker::Cache::reservedIndices, cache.Struct}}).second; !res)
    //             {
    //                 log << Logger::Error << "Duplicate symbol: " << sym.first.name << '\n';
    //                 throw std::runtime_error("Duplicate symbol");
    //             }
    //         }
    //     for (auto &func : result.internalFunctions)
    //     {
    //         cache.functions.push_back(CachedFunction{.symbol = &func, .body = ({
    //                                                                       auto res = result.FunctionBodies.find(func.name.name);
    //                                                                       res == result.FunctionBodies.end() ? nullptr : res->second;
    //                                                                   }),
    //                                                  .internal = true});
    //         if (auto res = cache.map.insert({{func.name}, {cache.functions.size() - 1, cache.Function}}).second; !res)
    //         {
    //             log << Logger::Error << "Duplicate symbol: " << func.name.name << " in module " << func.name.module_ << '\n';
    //             throw std::runtime_error("Duplicate symbol");
    //         }
    //     }
    //     for (auto &struct_ : result.internalStructs)
    //     {
    //         cache.structs.push_back(CachedStruct{.symbol = &struct_, .internal = true});
    //         if (auto res = cache.map.insert({{struct_.name}, {cache.structs.size() - 1 + Linker::Cache::reservedIndices, cache.Struct}}).second; !res)
    //         {
    //             log << Logger::Error << "Duplicate symbol: " << struct_.name.name << " in module " << struct_.name.module_ << '\n';
    //             throw std::runtime_error("Duplicate symbol");
    //         }
    //     }

    //     for (auto &struc : cache.structs)
    //         getStructInfo(struc, cache.structs);
    //     for (auto &func : cache.functions)
    //         finishFunctionCache(func, cache.structs);

    //     return cache;
    // }

    // This function goes through the function body and replaces all symbol names with their index in the cache and checks if they are at least of the correct kind(function or type).
    // This function calls itself recursively
    void UpdateVarUsage(const Linker::Cache &cache, Node &node, Logger &log)
    {
        switch (node.type)
        {
        case Node::Type::Type:
        case Node::Type::SizeOf:
        {
            auto &n = std::get<Linker::VarType>(node.value);
            if (node.children.size() != 0)
            {
                log << Logger::Error << "Type node with children\n";
                throw std::runtime_error("Type node should have no children");
            }
            auto t = cache.setPointerDepth(cache.typeFromId(n.name), n.pointerDepth);
            if (!~t)
            {
                log << Logger::Error << "Unknown type: " << n.name.name << " in module" << n.name.module_ << '\n';
                throw std::runtime_error("Unknown type");
            }
            if (t & cache.typeMask)
            {
                log << Logger::Error << "Type not found: " << n.name.name << " in module" << n.name.module_ << " It is a function";
                throw std::runtime_error("Type not found");
            }
            node.value = t;
            break;
        }
        default:
        {
            for (auto &child : node.children)
                UpdateVarUsage(cache, child, log);
            break;
        }
        }
    }

    // TODO: deduplicate constant pool:
    // TODO: make sure the chache exists at this point

    void compileMod(Linker::Module &module, Linker &linker, Logger &log)
    {
        log << Logger::Debug << "Generating cache...\n";
        for (auto &func : module.exports)
        {
            if (auto it = std::get_if<Linker::Symbol::Function>(&func.data))
                UpdateVarUsage(linker.cache, *static_cast<Node *>(it->node), log);
        }
        GenModBc(module, linker, log);
    }

    std::vector<Linker::Module *> compile(std::vector<Linker::Module> pass1, Linker &linker, Logger &log)
    {
        log << Logger::Info << "Beginning compilation\n";
        // TODO: create a mapping of all symbols, not just exported ones, to their cached values; Or not, since I now want everything to be exported
        std::vector<Linker::Module *> modules;

        log << Logger::Debug << "Processing modules\n";
        for (auto &mod : pass1)
        {
            log << Logger::Debug << "Processing module " << mod.name << "\n";
            for (auto &sym : mod.exports)
                sym.name.module_ = mod.name;
            modules.push_back(linker.provideModule(std::move(mod)));
        }
        log << Logger::Debug << "Locating imports\n";
        linker.satisfyDependencies();//There is no point in having this here, is there?
        //It would make more sense to load the symbols, as we encounter them so that compilation can finish even without loading all dependencies
        linker.cache.generate(linker);
        for (size_t i = 0; i < modules.size(); i++)
        {
            log << Logger::Info << "Compiling module " << modules[i]->name << "\n";
            compileMod(*modules[i], linker, log);
        }
        log << Logger::Info << "Compilation complete\n";
        std::vector<const Linker::Module *> result;
        return modules;
    }
}