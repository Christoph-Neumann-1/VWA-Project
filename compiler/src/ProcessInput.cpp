#include <ProcessInput.hpp>
#include <GenerateByteCode.hpp>

// TODO: replace all types with ids and remember to save them in cached structs
// TODO: remove copy constructors
// FIXME: I have somehow broken structs
namespace vwa
{

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
                log << Logger::Error << "Unknown type: " << n.name.name << " in module" << n.name.module << '\n';
                throw std::runtime_error("Unknown type");
            }
            if (t & cache.typeMask)
            {
                log << Logger::Error << "Type not found: " << n.name.name << " in module" << n.name.module << " It is a function";
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

    void recurseStruct(Linker::Module &module, Linker &linker, Logger &log, Linker::Cache::CachedType struc)
    {
        if ((struc & Linker::Cache::indexMask) < Linker::Cache::reservedIndicies)
            return;
        auto &s = linker.cache.structs[Linker::Cache::indexMask & struc - Linker::Cache::reservedIndicies];
        if (!~s.permIndex)
        {
            //This will add some redundant calls, but reduce runtime bloat
            if (s.symbol->name.module.size() && s.symbol->name.module != module.name)
            {
                s.permIndex = module.requiredSymbols.size();
                module.requiredSymbols.push_back(*s.symbol);
            }
            for (auto &m : s.members)
                recurseStruct(module, linker, log, Linker::Cache::getIndex(m.type));
        }
    }

    /**
     * @brief Replace internal functions with relative jumps and replace all external calls with more permanent indidices
     *
     */
    void LinkInternal(Linker::Module &module, Linker &linker, Logger &log)
    {
        // Idea: add an index property either to the cache or to the linker, then use that to indicate whether an item was already added, if so, where. Use this to minimize allocations by preallocationg chunks.
        // About structs: add recursive function, mark as used, process children unless already marked. Used on parameters/return types
        log << Logger::Info << "Beginning linking (stage 1)\n";

        linker.cache.resetIndices();
        auto &data = std::get<std::vector<bc::BcToken>>(module.data);
        auto end = data.data() + data.size();
        for (auto pos = module.offset + data.data(); pos < end;)
        {
            switch (pos->instruction)
            {
            case bc::CallFunc:
            {
                uint64_t &payload = *reinterpret_cast<uint64_t *>(pos + 1);
                auto &func = linker.cache.functions[payload];
                log << Logger::Debug << func.symbol->name << '\n';
                // auto &f = std::get<Linker::Symbol::Function>(func.symbol->data);
                // TODO: figure out how I intended to figure out when something is in the same module and check
                // Am I using jumprel at all? It would definitely remove space usage
                // whether it is more efficient to link internal jumps here than to have the linker do it at RT. Slighlty higher overhead at start that way, but jumps use less clock cycles
                if (!~func.permIndex)
                {
                    func.permIndex = module.requiredSymbols.size();
                    module.requiredSymbols.push_back(*func.symbol);
                }
                payload = func.permIndex;
            }
            default:
                pos += bc::getInstructionSize(pos->instruction);
            }
        }
        auto reqs = module.requiredSymbols.size();
        for (size_t i = 0; i < reqs; i++)
        {
            auto &func = std::get<Linker::Symbol::Function>(std::get<vwa::Linker::Symbol>(module.requiredSymbols[i]).data);
            recurseStruct(module, linker, log, linker.cache.typeFromId(func.returnType.name));
            for (auto &p : func.params)
                recurseStruct(module, linker, log, linker.cache.typeFromId(p.type.name));
        }
        auto exps = module.exports.size();
        for (size_t i = 0; i < exps; i++)
        {
            if (std::holds_alternative<Linker::Symbol::Function>(module.exports[i].data))
            {
                auto &func = std::get<Linker::Symbol::Function>(module.exports[i].data);
                recurseStruct(module, linker, log, linker.cache.typeFromId(func.returnType.name));
                for (auto &p : func.params)
                    recurseStruct(module, linker, log, linker.cache.typeFromId(p.type.name));
            }
            else
                recurseStruct(module, linker, log, linker.cache.typeFromId(module.exports[i].name));
        }
        log << Logger::Info << "stage 1 done\n";
    }

    void compileMod(Linker::Module &module, Linker &linker, Logger &log)
    {
        log << Logger::Debug << "Generating cache...\n";
        for (auto &func : module.exports)
        {
            if (auto it = std::get_if<Linker::Symbol::Function>(&func.data))
                UpdateVarUsage(linker.cache, *static_cast<Node *>(it->node), log);
        }
        GenModBc(module, linker, log);
        LinkInternal(module, linker, log);
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
                sym.name.module = mod.name;
            modules.push_back(linker.provideModule(std::move(mod)));
        }
        log << Logger::Debug << "Locating imports\n";
        linker.satisfyDependencies(); // There is no point in having this here, is there?
        // It would make more sense to load the symbols, as we encounter them so that compilation can finish even without loading all dependencies
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