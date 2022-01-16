#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <variant>
#include <Bytecode.hpp>

// TODO: consider memory mapping modules

namespace vwa
{
    // TODO: make the linker calculate and store the size of symbols to avoid redundant computation
    class Linker
    {

    public:
        using FFIFunc = void (*)(uint8_t *);

        struct Module
        {
            std::variant<std::monostate, void *, std::vector<bc::BcToken>> data;

            struct Symbol
            {
                std::string name;
                struct Function
                {
                    enum Type
                    {
                        Unlinked,
                        Internal,
                        External,
                    };

                    Type type = Unlinked;
                    union
                    {
                        size_t index;
                        bc::BcToken *address;
                        FFIFunc ffi;
                    };
                    // TODO: consider storing const
                    struct Parameter
                    {
                        std::string type;
                        uint64_t pointerDepth;
                    };
                    std::vector<Parameter> parameters;
                    Parameter returnType;
                    bool constexpr_;
                };
                struct Struct
                {
                    struct Field
                    {
                        std::string type;
                        uint64_t pointerDepth;
                        bool mutable_;
                    };
                    std::vector<Field> fields;
                };

                std::variant<Function, Struct> data;
            };

            Symbol *main{0};

            std::vector<Symbol> exportedSymbols;

            std::vector<std::string> importedModules;
            std::vector<std::string> exportedImports;

            std::unordered_map<std::string, const Symbol *> symbols; // Mainly used for looking up during compile time and to ensure no name collisions are present.

            // In theory it would be enough to just store the function names, but this way I can also make sure that the defintions are still the same at runtime.
            std::vector<std::variant<Symbol, const Symbol *>> requiredSymbols; // This table is generated at compile time and used by the linker for fast lookup.

            void satisfyDependencies(Linker &linker);

            ~Module();
        };

        Linker() = default;

        // This function takes the module and inserts it into the module map, throwing a runtime error if it already exists
        // The return value is intended for the compiler to provide the implementation of functions once it finishes compiling
        Module &provideModule(std::string name, Module module);

        // This function either provides a reference to an in-memory module or loads a new one from disk. If it can't find the module it will throw a runtime error.
        // The function is only supposed to be called after providing in-memory modules, therefore it loads dependencies right away.
        const Module &requireModule(std::string name);

        // Imports symbols from other modules. This is a seperate function since the compiler may need to provide multiple modules before this can succeed.
        void satisfyDependencies();

        // This function goes over all loaded modules, loading their bytecode/shared library and patches the jump adresses for bytecode.
        void patchAddresses();

    private:
        std::unordered_map<std::string, Module> modules;
    };
};