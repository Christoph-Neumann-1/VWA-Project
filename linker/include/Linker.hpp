#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <variant>
#include <Bytecode.hpp>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <cstring>
#include <filesystem>

// TODO: consider memory mapping modules

// FIXME: when two modules have different definitions for a struct code calling into that module will still compile,
//  it should be an error. this requires changing the format of modules, so I am reluctant to do that.

// FIXME: this will break for two interdependant modules compiled at once, they need to export their definitions before beginning compilation
// After all symbols are exported and additional modules are loaded, we can build a cache and pass it to every module
// TODO: Is a cache even necessary? it adds a lot of complexity and hashmaps should be fast as well, right?

namespace vwa
{
    struct Identifier
    {
        std::string name;
        std::string module{}; // TODO: add a constructor

        bool operator==(const Identifier &other) const
        {
            return name == other.name && module == other.module;
        }
    };
    inline std::ostream &operator<<(std::ostream &s, const Identifier &i)
    {
        s << i.module << std::string(":") << i.name;
        return s;
    }
    // TODO: make the linker calculate and store the size of symbols to avoid redundant computation
    // TODO: store the position of function calls as possible optimization to reduce the amount of processing needed
    // TODO: store the beginning of the actual code as we don't want to patch the constant section
    class VM;
}

namespace std
{
    template <>
    struct hash<vwa::Identifier>
    {
        // I really hope this works
        size_t operator()(const vwa::Identifier &id) const
        {
            return hash<string>()(id.name) ^ hash<string>()(id.module);
        }
    };
}

namespace vwa
{

    class Logger;
    class Linker
    {

    public:
        using FFIFunc = void (*)(VM *vm);
        struct VarType
        {
            Identifier name{};
            uint32_t pointerDepth{};
            bool operator==(const VarType &other) const
            {
                return name == other.name && pointerDepth == other.pointerDepth;
            }
        };
        struct Symbol
        {
            Identifier name;
            struct Field
            {
                std::string name;
                VarType type;
                // bool isMutable{1}; // Ignored as of right now
                bool operator==(const Field &other) const
                {
                    return name == other.name && type == other.type;
                    // &&isMutable == other.isMutable;
                }
            };
            struct Function
            {
                enum Type
                {
                    Unlinked, // Declared but without any definition
                    Compiled, // This was compiled to bytecode, but it was never updated to use real adresses
                    Internal, // Definition is somewhere in bytecode
                    External, // Uses the foreign function interface
                };
                Type type = Unlinked;
                std::vector<Field> params;
                VarType returnType;
                // bool constexpr_ = false; // Idk if I am ever going to use this
                union
                {
                    char contents[16]{}; // This is here to set everything to zero
                    struct
                    {
                        union
                        {
                            FFIFunc ffi;
                            void *node;
                        };
                        union
                        {
                            void *ffi_direct;
                            bc::BcToken *bcAddress;
                            size_t idx;
                        }; // This is used when linking together two ffi modules, they can just call each other directly without relying on the stack. This may be null, in that case a wrapper is generated instead
                    };
                };
                // Does not check if it points to the same definition
                bool operator==(const Symbol::Function &other) const
                {
                    if (params.size() != other.params.size())
                        return false;
                    for (size_t i = 0; i < params.size(); i++)
                        if (params[i].type != other.params[i].type)
                            return false;
                    return returnType == other.returnType;
                }
            };

            struct Struct
            {
                std::vector<Field> fields;
                bool operator==(const Symbol::Struct &other) const
                {
                    return fields == other.fields;
                }
            };
            std::variant<Function, Struct> data;

            bool operator==(const Symbol &other) const
            {
                if (name != other.name)
                    return false;
                if (data.index() != other.data.index())
                    return false;
                if (data.index() == 0)
                    return std::get<Function>(data) == std::get<Function>(other.data);
                else
                    return std::get<Struct>(data) == std::get<Struct>(other.data);
            }
        };

        struct Module
        {
            std::string name;
            struct DlHandle
            {
                void *data;

                DlHandle(DlHandle &&other)
                {
                    data = other.data;
                    other.data = nullptr;
                }
                DlHandle &operator=(DlHandle &&other)
                {
                    data = other.data;
                    other.data = nullptr;
                    return *this;
                }
                DlHandle(DlHandle &) = delete;
                DlHandle &operator=(DlHandle) = delete;
                DlHandle(void *d) : data(d) {}
                ~DlHandle();
                operator void *() const
                {
                    return data;
                }

                // TODO: move semantics
            };

            // Bytecode is stored using mmap. If there is no backing file, then you need to allocate some space with mmap
            struct Mapping
            {
                static const long pageSize;
                bc::BcToken *data;
                size_t offset; // Since offset in mmap needs to be a multiple of page size, data may not be at the beginning of the mapping
                size_t size;   // Size of the data NOT the mapping for that you need to add offset
                ~Mapping();
            };
            std::variant<std::monostate, DlHandle, Mapping, std::vector<bc::BcToken>> data;
            size_t offset{~0ul}; // Offset where the actual code starts, before that are the constants
            // TODO: how do I avoid duplicates here?
            std::vector<std::string> imports; // This is not used at runtime;
            std::vector<std::variant<Symbol, Symbol *>> requiredSymbols;
            std::vector<Symbol> exports; // should I rename this?

            void patchAddresses(Linker &linker);
            void satisfyDependencies(Linker &linker);
            bool isFFI()
            {
                return std::holds_alternative<DlHandle>(data);
            }
            bc::BcToken *getData()
            {
                switch (data.index())
                {
                case 2:
                    return std::get<2>(data).data;
                case 3:
                    return std::get<3>(data).data();
                default:
                    throw std::runtime_error("Not bytecode");
                }
            }
            const bc::BcToken *getData() const
            {
                switch (data.index())
                {
                case 2:
                    return std::get<2>(data).data;
                case 3:
                    return std::get<3>(data).data();
                default:
                    throw std::runtime_error("Not bytecode");
                }
            }
            size_t getDataSize() const
            {
                switch (data.index())
                {
                case 2:
                    return std::get<2>(data).size;
                case 3:
                    return std::get<3>(data).size();
                default:
                    throw std::runtime_error("Not bytecode");
                }
            }
            bool operator==(const Module &other)
            {
                if (name != other.name || offset != other.offset || requiredSymbols != other.requiredSymbols || exports != other.exports || data.index() != other.data.index())
                    return false;
                if (std::holds_alternative<std::monostate>(data) || (std::holds_alternative<DlHandle>(data) && std::get<DlHandle>(data).data == std::get<DlHandle>(other.data).data))
                    return true;
                if (getDataSize() != other.getDataSize())
                    return false;
                return !std::memcmp(getData(), other.getData(), getDataSize());
            }
            /* TODO
            Do I need hashes for faster comparisons?
            How do I handle struct Definitions? Do I need to store the entire definition in every module to make sure it is the same at runtime?(Recursively store all structs in use)
            */
        };

        // This function takes the module and inserts it into the module map, throwing a runtime error if it already exists
        // The return value is intended for the compiler to provide to the implementation of functions once it finishes compiling
        Module *provideModule(Module module);

        // Imports symbols from other modules. This is a seperate function since the compiler may need to provide multiple modules before this can succeed.
        void satisfyDependencies();

        // This function goes over all loaded modules, loading their bytecode/shared library and patches the jump adresses for bytecode.
        void patchAddresses();

        void loadModule(const std::string &name);

        const bc::BcToken *findMain()
        {
            const bc::BcToken *ret{0};
            for (auto &module : modules)
                for (auto &e : module.second.exports)
                    if (e.name.name == "main")
                    {
                        if (ret)
                            return reinterpret_cast<const bc::BcToken *>(~0ul);
                        ret = std::get<Symbol::Function>(e.data).bcAddress;
                    }
            return ret;
        }

        struct SymbolNotFound : std::exception
        {
            SymbolNotFound(const std::string &name) : name(name) {}
            std::string name;
            const char *what() const noexcept override
            {
                return name.c_str();
            }
        };

        Symbol &getSymbol(const Identifier &name);
        Symbol *tryFind(const Identifier &name);
        Module &getModule(const std::string &name);
        Module *tryFind(const std::string &name);
        void requireMatch(const Symbol &symbol) // Locates the symbol and checks if it is equal to the one provided
        {
            auto sym = getSymbol(symbol.name);
            if (sym != symbol)
                throw SymbolNotFound(symbol.name.name);
        }

        std::string serialize(const Module &,bool);
        static Module deserialize(std::string_view in);
        std::string generateInterface();
        // TODO: try to transfer some of these ideas to the vm
        struct Cache
        {
            // On clang the performance is identical to bitfields, but on gcc it is a lot faster. Gcc is slow in both cases btw
            static constexpr uint64_t typeMask = 0b1ul << 63; // 1 means function,0 struct
            static constexpr uint64_t indexMask = 0xfffffffful;
            static constexpr uint64_t pointerDepthMask = 0x7FFFFFFFul << 32;
            static constexpr uint64_t reservedIndicies = 5ul;

            static constexpr uint64_t invalidType = ~0ul;

            // Last bit indicates the type of variable, first 32 bits are the index, next 31 bits pointerdepth
            // Pointerdepth is meaningless for functions as of right now
            using CachedType = uint64_t;
            // Can directly be used as a type
            std::unordered_map<Identifier, CachedType> ids; // The last bit is used as a type
            enum class SymbolType : uint8_t
            {
                Function,
                Struct,
                Primitive,
            };
            enum PrimitiveType : uint8_t
            {
                Void,
                I64,
                F64,
                U8,
                FPtr,
            };
            struct CachedStruct
            {
                Symbol *symbol{};
                struct Member
                {
                    CachedType type;
                    // I don't need such a large offset, but it padding makes it larger anyways
                    // Technically offset isn't even necessary
                    uint64_t offset{~0ul};
                };
                std::vector<Member> members{};
                uint32_t size{~0u};
                enum State : uint8_t
                {
                    Uninitialized,
                    Processing,
                    Done
                };
                State state{Uninitialized};
                uint64_t permIndex{~0ul};
                // There is no usage counter in here, every module needs to keep track of that itself
                // I might add an export flag later so that not everything is visible everywhere
            };
            struct CachedFunction
            {
                Symbol *symbol{};
                std::vector<CachedType> params{};
                CachedType returnType{};
                uint64_t permIndex{~0ul};
            };
            std::vector<CachedStruct> structs;
            std::vector<CachedFunction> functions;

            void resetIndices()
            {
                for (auto &s : structs)
                    s.permIndex = -1;
                for (auto &f : functions)
                    f.permIndex = -1;
            }

            // Returns the index of the member if found, otherwise ~0
            uint32_t getMemberByName(CachedType type, const std::string &name) const
            {
                // This can be optimized by adding the cache index to the symbol map, if necessary
                auto sym = structs[(type & indexMask) - reservedIndicies].symbol;
                auto &struct_ = std::get<1>(sym->data);
                if (auto it = std::find_if(struct_.fields.begin(), struct_.fields.end(), [&name](const Symbol::Field &mem)
                                           { return mem.name == name; });
                    it != struct_.fields.end())
                    return std::distance(struct_.fields.begin(), it);
                return ~0u;
            }

            // Assumes type to be valid and of type struct
            uint32_t getSizeOfType(CachedType type) const
            {
                if (type & pointerDepthMask)
                    return 8;
                switch (type & indexMask)
                {
                case PrimitiveType::Void:
                    return 0;
                case PrimitiveType::I64:
                case PrimitiveType::F64:
                    return 8;
                case PrimitiveType::U8:
                    return 1;
                case PrimitiveType::FPtr:
                    return 8; // Might change later
                default:
                    return structs[(type & indexMask) - reservedIndicies].size;
                }
            }
            // Assumes the type is valid
            static constexpr SymbolType getType(CachedType type)
            {
                if (type & typeMask)
                    return SymbolType::Function;
                return (type & indexMask) < reservedIndicies ? SymbolType::Primitive : SymbolType::Struct;
            }
            bool isTypeValid(CachedType type) const
            {
                if (type & typeMask)
                    return (type & indexMask) < functions.size() && ~(type & indexMask) && !(type & pointerDepthMask);
                return (type & indexMask) < (structs.size() + reservedIndicies) && ~(type & indexMask) && ~(type & pointerDepthMask);
            }
            // I don't know if I am ever going to use these, but they are a nice reminder
            static constexpr uint64_t getIndex(CachedType type)
            {
                return type & indexMask;
            }
            static constexpr uint64_t getPointerDepth(CachedType type)
            {
                return (type & pointerDepthMask) >> 32;
            }
            static constexpr CachedType constructType(SymbolType type, uint64_t index, uint64_t pointerDepth)
            {
                return (type == SymbolType::Function ? 1ul : 0ul) | index | ((pointerDepth << 32) & pointerDepthMask);
            }

            CachedType typeFromId(const Identifier &id) const
            {
                // This can probably be removed after profiling
                // TODO: btw, where am I checking if something is a keyword?
                // if (id.name == "void")
                //     return Void;
                // if (id.name == "int")
                //     return I64;
                // if (id.name == "float")
                //     return F64;
                // if (id.name == "char")
                //     return U8;
                // if (id.name == "function")
                //     return FPtr;
                if (auto it = ids.find(id); it != ids.end())
                    // return it->second & typeMask ? invalidType : it->second; so maybe this served some purpose, but it is causing problems
                    // FIXME: I realized that setting the pointer depth directly is unsafe when using aliased types, like strings
                    return it->second;
                return invalidType;
            }

            CachedType typeFromVarType(const VarType &type) const
            {
                auto t = typeFromId(type.name);
                return setPointerDepth(t, getPointerDepth(t) + type.pointerDepth);
            }

            static constexpr CachedType setPointerDepth(CachedType type, uint32_t depth)
            {
                return (type & ~pointerDepthMask) | ((uint64_t{depth} << 32) & pointerDepthMask);
            }

            // type,pointerDepth,index
            static constexpr std::tuple<bool, uint32_t, uint32_t> disassemble(CachedType t)
            {
                return {t & typeMask, (t & pointerDepthMask) >> 32, t & indexMask};
            }

            void generate(Linker &linker);
        };

        Cache cache;

        void addSearchPath(std::string_view path)
        {
            searchPaths.push_back(path);
        }

        // TODO: use
        void fillMissingModuleNames(Module &m)
        {
            auto patch = [&m, this](Identifier &id)
            {
                if (id.module.empty() && symbols.find({id.name, m.name}) != symbols.end())
                    id.module = m.name;
            };
            auto patchSym = [&patch](Symbol &sym)
            {
                patch(sym.name);
                if (std::holds_alternative<Symbol::Struct>(sym.data))
                    for (auto &f : std::get<Symbol::Struct>(sym.data).fields)
                        patch(f.type.name);
                else
                {
                    auto &f = std::get<Symbol::Function>(sym.data);
                    patch(f.returnType.name);
                    for (auto &p : f.params)
                        patch(p.type.name);
                }
            };
            for (auto &sym : m.exports)
                patchSym(sym);
            for (auto &sym : m.requiredSymbols)
                if (auto s = std::get_if<Symbol>(&sym))
                    patchSym(*s);
        }

    private:
        std::unordered_map<std::string, Module> modules;
        std::unordered_map<Identifier, Symbol *> symbols;
        std::vector<std::filesystem::path> searchPaths{"."};
    };
};