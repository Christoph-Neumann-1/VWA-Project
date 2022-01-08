#include <Compiler.hpp>

// TODO: replace all types with ids and remember to save them in cached structs

namespace vwa
{
    // Used for storing the true location of variables
    // If I implement stack pointer free functions I might also store more information
    struct Scope
    {
        uint64_t size;
        uint64_t offset; // From the stack pointer
        // TODO: UUID for variables
        std::unordered_map<std::string, uint64_t> variables;
    };

    // This is used by the compiler for fast look up of struct sizes.
    // Member names get replaced by indices while processing the function, they are then used to look up their offset.
    // Currently this doesn't help much since I am only doing a single pass, but once I start adding optimizations it will reduce the cost of lookups.
    struct CachedStruct
    {
        std::vector<size_t> memberOffsets{};
        size_t size = -1;
        size_t refCount = 0;                     // This is used to remove dependencies on structs which are not actually used in the module.
        const Linker::Module::Symbol *symbol{0}; // Used if the name or type of the struct needs to be retrieved again, like for error reporting.
        bool internal : 1;
        enum State
        {
            Uninitialized,
            Processing,
            Finished,
        };
        State state : 2 = Uninitialized;
        constexpr static size_t npos = -1;
    };

    // This is used by the compiler to replace known functions with their adresses and remove unused functions.
    struct CachedFunction
    {
        // I did not want to waste addtional space on loop detection, so I just repurposed the unused space in booleans
        uint64_t refCount = 0;
        const Linker::Module::Symbol *symbol{0};
        bool internal;
    };

    struct ProcessingResult
    {
        Linker::Module *module;
        std::vector<Linker::Module::Symbol> internalStructs;
        std::vector<Linker::Module::Symbol> internalFunctions;
    };

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
            struc.memberOffsets.push_back(struc.size);
            if (member.type == "i32")
            {
                struc.size += 4;
            }
            else if (member.type == "i64")
            {
                struc.size += 8;
            }
            else if (member.type == "f32")
            {
                struc.size += 4;
            }
            else if (member.type == "f64")
            {
                struc.size += 8;
            }
            else if (member.type == "char")
            {
                struc.size += 1;
            }
            else if (member.type == "bool")
            {
                struc.size += 1;
            }
            else
            {
                auto it = std::find_if(structs.begin(), structs.end(), [&member](const CachedStruct &s)
                                       { return s.symbol->name == member.type; });
                if (it == structs.end())
                    throw std::runtime_error("Could not find struct: " + member.type);
                auto mem = getStructInfo(*it, structs);
                struc.size += mem.size;
            }
        }
        struc.state = CachedStruct::Finished;
        return struc;
    }

    std::pair<std::vector<CachedStruct>, std::vector<CachedFunction>> generateCache(const ProcessingResult &result)
    {
        // TODO: I need to make this more efficient, linear search through the entire list of structs is horrible.
        std::vector<CachedStruct> structs;
        std::vector<CachedFunction> functions;

        for (auto &sym : result.module->symbols)
            if (std::holds_alternative<Linker::Module::Symbol::Function>(sym.second->data))
                functions.push_back(CachedFunction{.symbol = sym.second, .internal = false});
            else
                structs.push_back(CachedStruct{.symbol = sym.second, .internal = false});
        for (auto &func : result.internalFunctions)
            functions.push_back(CachedFunction{.symbol = &func, .internal = true});
        for (auto &struct_ : result.internalStructs)
            structs.push_back(CachedStruct{.symbol = &struct_, .internal = true});

        for (auto &struc : structs)
            getStructInfo(struc, structs);

        return {std::move(structs), std::move(functions)};
    }

    void compileMod(const ProcessingResult &result)
    {

        auto [cachedStructs, cachedFunctions] = generateCache(result);
    }

    std::vector<const Linker::Module *> compile(std::vector<std::pair<std::string, Pass1Result>> pass1, Linker &linker)
    {
        // TODO: create a mapping of all symbols, not just exported ones, to their cached values;
        std::vector<ProcessingResult> modules;
        for (auto &[name, mod] : pass1)
        {
            ProcessingResult result;
            Linker::Module module;
            // TODO duplicate checking
            // TODO: move semantics
            for (auto &fun : mod.functions)
            {
                std::vector<Linker::Module::Symbol::Function::Parameter> parameters;
                for (auto &param : fun.parameters)
                    parameters.push_back({param.type.name, param.type.pointerDepth});
                Linker::Module::Symbol sym{
                    fun.name, Linker::Module::Symbol::Function{Linker::Module::Symbol::Function::Unlinked, ulong{0} - 1, std::move(parameters), fun.constexpr_}};
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
        linker.satisfyDependencies();
        for (size_t i = 0; i < modules.size(); i++)
        {
            compileMod(modules[i]);
        }
        std::vector<const Linker::Module *> result;
        for (auto &mod : modules)
            result.push_back(mod.module);
        return result;
    }
}