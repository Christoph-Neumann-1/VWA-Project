#include <Compiler.hpp>

// TODO: replace all types with ids and remember to save them in cached structs
// TODO: remove copy constructors
namespace vwa
{

    static constexpr size_t numReservedIndices = 6;

    enum PrimitiveTypes
    {
        Void,
        I32,
        I64,
        F32,
        F64,
        U8, // Both char and bool, no point creating two types
    };

    // Used for storing the true location of variables
    // If I implement stack pointer free functions I might also store more information
    struct Scope
    {
        uint64_t size;
        struct Variable
        {
            uint64_t offset;
            uint64_t type;
        };
        // TODO: UUID for variables
        std::unordered_map<std::string, Variable>
            variables;
    };

    // This is used by the compiler for fast look up of struct sizes.
    // Member names get replaced by indices while processing the function, they are then used to look up their offset.
    // Currently this doesn't help much since I am only doing a single pass, but once I start adding optimizations it will reduce the cost of lookups.
    struct CachedStruct
    {
        struct Member
        {
            size_t offset = -1;
            size_t type = -1; // Not actually an index, the first few values are reserved for primitive types
            size_t ptrDepth = -1;
        };
        size_t size = -1;
        std::vector<Member> members{};
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
        struct Parameter
        {
            size_t type = -1, pointerDepth = -1;
        };
        // I did not want to waste addtional space on loop detection, so I just repurposed the unused space in booleans
        std::vector<Parameter> args{};
        Parameter returnType;
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

    size_t typeFromString(const std::string &str, const std::vector<CachedStruct> &structs)
    {
        if (str == "void")
            return Void;
        if (str == "i32")
            return I32;
        if (str == "i64")
            return I64;
        if (str == "f32")
            return F32;
        if (str == "f64")
            return F64;
        if (str == "char" || str == "bool")
            return U8;

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
        case F32:
        case I32:
            return 4;
        case F64:
        case I64:
            return 8;
        case U8:
            return 1;
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
        auto it = std::find_if(structs.begin(), structs.end(), [&f](const CachedStruct &s)
                               { return s.symbol->name == f.returnType.type; });
        func.returnType.type = typeFromString(f.returnType.type, structs);
        func.returnType.pointerDepth = f.returnType.pointerDepth;
        for (auto &param : f.parameters)
        {
            func.args.push_back({typeFromString(param.type, structs), param.pointerDepth});
        }
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
        for (auto &func : functions)
            finishFunctionCache(func, structs);

        return {std::move(structs), std::move(functions)};
    }

    // TODO: deduplicate constant pool:

    void compileMod(const ProcessingResult &source)
    {
        auto [cachedStructs, cachedFunctions] = generateCache(source);
    }

    std::vector<const Linker::Module *> compile(std::vector<std::pair<std::string, Pass1Result>> pass1, Linker &linker)
    {
        Linker::Module mod;
        mod.exportedSymbols.push_back(Linker::Module::Symbol{.name = "foo", .data = Linker::Module::Symbol::Function{.returnType = {"void", 0}}});
        linker.provideModule("test", mod);
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