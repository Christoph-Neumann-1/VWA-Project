#include <ProcessInput.hpp>

// TODO: replace all types with ids and remember to save them in cached structs
// TODO: remove copy constructors
namespace vwa
{
    size_t typeFromString(const std::string &str, const std::vector<CachedStruct> &structs)
    {
        if (str == "void")
            return Void;
        if (str == "int")
            return I64;
        if (str == "float")
            return F64;

        auto it = std::find_if(structs.begin(), structs.end(), [&str](const CachedStruct &s)
                               { return s.symbol->name == str; });
        if (it == structs.end())
            throw std::runtime_error("Could not find struct: " + str);
        return numReservedIndices + std::distance(structs.begin(), it);
    }

    size_t getSizeOfType(size_t t, const std::vector<CachedStruct> &structs)
    {
        switch (t)
        {
        case Void:
            return 0;
        case F64:
        case I64:
            return 8;
        default:
            return structs[t - numReservedIndices].size;
        }
    }

    // Returns the size of the struct in bytes, if it has not been calculated it is stored.
    // If a cycle is detected an exception is thrown
    const CachedStruct &getStructInfo(CachedStruct &struc, std::vector<CachedStruct> &structs)
    {
        if (struc.state == CachedStruct::Processing)
            throw std::runtime_error("Error: cycle detected."); // TODO: proper backtrace
        if (struc.state == CachedStruct::Finished)
            return struc;
        struc.size = 0;
        struc.state = CachedStruct::Processing;
        for (auto &member : std::get<Linker::Module::Symbol::Struct>(struc.symbol->data).fields)
        {
            auto type = typeFromString(member.type, structs);
            struc.members.push_back({struc.size, type, member.pointerDepth});
            if (type < numReservedIndices)
            {
                // We can't use this function for structs,since it is unknown whether their size has been calculated yet
                struc.size += getSizeOfType(type, structs);
                continue;
            }
            auto &mem = getStructInfo(structs[type - numReservedIndices], structs);
            struc.size += mem.size;
        }
        struc.state = CachedStruct::Finished;
        return struc;
    }

    void finishFunctionCache(CachedFunction &func, const std::vector<CachedStruct> &structs)
    {
        auto &f = std::get<Linker::Module::Symbol::Function>(func.symbol->data);
        func.returnType = {typeFromString(f.returnType.type, structs), f.returnType.pointerDepth};
        for (auto &param : f.parameters)
        {
            func.args.push_back({typeFromString(param.type, structs), param.pointerDepth});
        }
    }

    Cache generateCache(const ProcessingResult &result, Logger &log)
    {
        // TODO: I need to make this more efficient, linear search through the entire list of structs is horrible.
        Cache cache;
        cache.map.insert({"void", {Void, Cache::Type::Struct}});
        cache.map.insert({"int", {I64, Cache::Type::Struct}});
        cache.map.insert({"float", {F64, Cache::Type::Struct}});

        for (auto &sym : result.module->symbols)
            if (std::holds_alternative<Linker::Module::Symbol::Function>(sym.second->data))
            {
                cache.functions.push_back(CachedFunction{.symbol = sym.second, .body = ({ auto res = result.FunctionBodies.find(sym.first); res==result.FunctionBodies.end()?nullptr:res->second; }), .internal = false});
                if (auto res = cache.map.insert({sym.first, {cache.functions.size() - 1, cache.Function}}).second; !res)
                {
                    log << Logger::Error << "Duplicate symbol: " << sym.first << '\n';
                    throw std::runtime_error("Duplicate symbol");
                }
            }
            else
            {
                cache.structs.push_back(CachedStruct{.symbol = sym.second, .internal = false});
                if (auto res = cache.map.insert({sym.first, {cache.structs.size() - 1 + numReservedIndices, cache.Struct}}).second; !res)
                {
                    log << Logger::Error << "Duplicate symbol: " << sym.first << '\n';
                    throw std::runtime_error("Duplicate symbol");
                }
            }
        for (auto &func : result.internalFunctions)
        {
            cache.functions.push_back(CachedFunction{.symbol = &func, .body = ({ auto res = result.FunctionBodies.find(func.name); res==result.FunctionBodies.end()?nullptr:res->second; }), .internal = true});
            if (auto res = cache.map.insert({func.name, {cache.functions.size() - 1, cache.Function}}).second; !res)
            {
                log << Logger::Error << "Duplicate symbol: " << func.name << '\n';
                throw std::runtime_error("Duplicate symbol");
            }
        }
        for (auto &struct_ : result.internalStructs)
        {
            cache.structs.push_back(CachedStruct{.symbol = &struct_, .internal = true});
            if (auto res = cache.map.insert({struct_.name, {cache.structs.size() - 1 + numReservedIndices, cache.Struct}}).second; !res)
            {
                log << Logger::Error << "Duplicate symbol: " << struct_.name << '\n';
                throw std::runtime_error("Duplicate symbol");
            }
        }

        for (auto &struc : cache.structs)
            getStructInfo(struc, cache.structs);
        for (auto &func : cache.functions)
            finishFunctionCache(func, cache.structs);

        return cache;
    }

    // This function goes through the function body and replaces all symbol names with their index in the cache and checks if they are at least of the correct kind(function or type).
    // This function calls itself recursively
    void UpdateVarUsage(const Cache &cache, Node &node, Logger &log)
    {
        switch (node.type)
        {
        case Node::Type::Type:
        {
            auto &n = std::get<Node::VarType>(node.value);
            if (node.children.size() != 0)
            {
                log << Logger::Error << "Type node with children\n";
                throw std::runtime_error("Type node should have no children");
            }
            auto it = cache.map.find(n.name);
            if (it == cache.map.end())
            {
                log << Logger::Error << "Unknown type: " << n.name << '\n';
                throw std::runtime_error("Unknown type");
            }
            if (it->second.second != Cache::Type::Struct)
            {
                log << Logger::Error << "Type not found: " << n.name << " It is a function";
                throw std::runtime_error("Type not found");
            }
            node.value = Node::VarTypeCached{it->second.first, n.pointerDepth};
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

    void compileMod(const ProcessingResult &source, Logger &log)
    {
        log << Logger::Debug << "Generating cache...\n";
        auto cache = generateCache(source, log);
        for (auto &func : cache.functions)
        {
            if (func.body)
            {
                log << Logger::Debug << "Updating function body: " << func.symbol->name << '\n';
                UpdateVarUsage(cache, *func.body, log);
            }
            else
            {
                log << Logger::Debug << "Skipping function: " << func.symbol->name << ". Reason: no body\n";
            }
        }
    }

    std::vector<const Linker::Module *> compile(std::vector<std::pair<std::string, Pass1Result>> pass1, Linker &linker, Logger &log)
    {
        log << Logger::Info << "Beginning compilation\n";
        log << Logger::Debug << "Injecting fake module\n";
        Linker::Module mod;
        mod.exportedSymbols.push_back(Linker::Module::Symbol{.name = "foo", .data = Linker::Module::Symbol::Function{.returnType = {"void", 0}}});
        linker.provideModule("test", mod);
        // TODO: create a mapping of all symbols, not just exported ones, to their cached values;
        std::vector<ProcessingResult> modules;

        log << Logger::Debug << "Processing modules\n";
        for (auto &[name, mod] : pass1)
        {
            log << Logger::Debug << "Processing module " << name << "\n";
            ProcessingResult result;
            Linker::Module module;
            // TODO duplicate checking
            // TODO: move semantics
            for (auto &fun : mod.functions)
            {
                if (auto res = result.FunctionBodies.insert({fun.name, &fun.body}).second; !res)
                {
                    log << Logger::Error << "Duplicate function: " << fun.name << '\n';
                    throw std::runtime_error("Duplicate function");
                }

                // TODO: this is doesn't handle pointer depth.
                std::vector<Linker::Module::Symbol::Function::Parameter> parameters;
                for (auto &param : fun.parameters)
                    parameters.push_back({param.type.name, param.type.pointerDepth});
                Linker::Module::Symbol sym{
                    fun.name, Linker::Module::Symbol::Function{Linker::Module::Symbol::Function::Unlinked, ulong{0} - 1, std::move(parameters), {fun.returnType.name, fun.returnType.pointerDepth}, fun.constexpr_}};
                if (fun.exported)
                    // TODO: remove copy
                    module.exportedSymbols.push_back(sym);
                result.internalFunctions.push_back(std::move(sym));
                if (fun.name == "main")
                {
                    if (module.main)
                        throw std::runtime_error("Multiple main functions");
                    module.main = &module.exportedSymbols.back();
                }
            }
            for (auto &struct_ : mod.structs)
            {
                std::vector<Linker::Module::Symbol::Struct::Field> fields;
                for (auto &field : struct_.fields)
                    fields.push_back({field.type.name, field.type.pointerDepth, field.isMutable});
                Linker::Module::Symbol sym{struct_.name, Linker::Module::Symbol::Struct{std::move(fields)}};
                if (struct_.exported)
                    module.exportedSymbols.push_back(sym);
                result.internalStructs.push_back(std::move(sym));
            }
            for (auto &import : mod.imports)
                if (import.exported)
                    module.exportedImports.push_back(import.name);
                else
                    module.importedModules.push_back(import.name);
            modules.push_back(({ result.module = &linker.provideModule(name, std::move(module)); std::move(result); }));
        }
        log << Logger::Debug << "Locating imports\n";
        linker.satisfyDependencies();
        for (size_t i = 0; i < modules.size(); i++)
        {
            log << Logger::Info << "Compiling module " << pass1[i].first << "\n";
            compileMod(modules[i], log);
        }
        log << Logger::Info << "Compilation complete\n";
        std::vector<const Linker::Module *> result;
        for (auto &mod : modules)
            result.push_back(mod.module);
        return result;
    }
}