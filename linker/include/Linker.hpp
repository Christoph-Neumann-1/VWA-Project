#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <variant>
#include <Bytecode.hpp>
#include <Node.hpp>

// TODO: consider memory mapping modules

// FIXME: when two modules have different definitions for a struct code calling into that module will still compile,
//  it should be an error. this requires changing the format of modules, so I am reluctant to do that.

namespace vwa
{
    // TODO: make the linker calculate and store the size of symbols to avoid redundant computation
    // TODO: store the position of function calls as possible optimization
    class VM;
    class Linker
    {

    public:
        // TODO: remove second parameter
        using FFIFunc = void (*)(VM *vm);

        struct Module
        {
            std::string name;
            // TODO: make this more efficient
            std::variant<std::monostate, void *, std::vector<bc::BcToken>> data;

            struct Symbol
            {
                Identifier name;
                struct Function
                {
                    enum Type
                    {
                        Unlinked,
                        Internal,
                        External,
                    };

                    Type type = Unlinked;
                    // This looks like this, because I got a missing field initializer warning, even though there was a default initializer.
                    union Implementation
                    {
                        size_t index;
                        bc::BcToken *address;
                        struct
                        {
                            FFIFunc ffi;
                            void *directLink; // I need to be careful here, because there might be conversions happening
                            // A way to circumwent that is to set it to null if that is the case and instead invoke the ffilink after pushing stuff to the stack.
                        };
                    } impl{0};
                    // TODO: consider storing const
                    struct Parameter
                    {
                        Identifier type;
                        uint64_t pointerDepth;
                    };
                    std::vector<Parameter> parameters{};
                    Parameter returnType;
                    bool constexpr_ = false;
                };
                struct Struct
                {
                    struct Field
                    {
                        // This should really not be here, but I don't intend to
                        // change the rest of the code anytime soon, so it is here to stay.
                        std::variant<Identifier, size_t> type;
                        std::string name;
                        uint32_t pointerDepth;
                        bool mutable_;
                    };
                    std::vector<Field> fields;
                };

                std::variant<Function, Struct> data;
            };

            size_t main{0}; // Offset by one

            std::vector<Symbol> exportedSymbols;

            std::vector<std::string> importedModules;

            std::unordered_map<Identifier, Symbol *> symbols; // Mainly used for looking up during compile time and to ensure no name collisions are present. TODO: replace by global table

            // In theory it would be enough to just store the function names, but this way I can also make sure that the defintions are still the same at runtime.
            std::vector<std::variant<Symbol, Symbol *>> requiredSymbols; // This table is generated at compile time and used by the linker for fast lookup.

            void satisfyDependencies(Linker &linker);

            ~Module();
        };

        Linker() = default;

        // This function takes the module and inserts it into the module map, throwing a runtime error if it already exists
        // The return value is intended for the compiler to provide the implementation of functions once it finishes compiling
        Module &provideModule(std::string name, Module module);

        // This function either provides a reference to an in-memory module or loads a new one from disk. If it can't find the module it will throw a runtime error.
        // The function is only supposed to be called after providing in-memory modules, therefore it loads dependencies right away.
        Module &requireModule(std::string name);

        // Imports symbols from other modules. This is a seperate function since the compiler may need to provide multiple modules before this can succeed.
        void satisfyDependencies();

        // This function goes over all loaded modules, loading their bytecode/shared library and patches the jump adresses for bytecode.
        void patchAddresses();

    private:
        std::unordered_map<std::string, Module> modules;
    };
};