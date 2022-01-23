#pragma once

#include <Compiler.hpp>

namespace vwa
{

    static constexpr size_t numReservedIndices = 3;

    // To make live easier for myself I decided that 2 types is good enough for now, if you are using this language you are clearly not worried about performance anyway
    enum PrimitiveTypes
    {
        Void,
        I64,
        F64,
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
        Parameter returnType{};
        uint64_t refCount = 0;
        const Linker::Module::Symbol *symbol{0};
        Node *body;
        bool internal;
    };

    struct Cache
    {
        enum Type
        {
            Struct,
            Function,
        };
        std::unordered_map<std::string, std::pair<size_t, Type>> map{};
        std::vector<CachedStruct> structs{};
        std::vector<CachedFunction> functions{};
    };

    struct ProcessingResult
    {
        Linker::Module *module;
        std::vector<Linker::Module::Symbol> internalStructs;
        std::vector<Linker::Module::Symbol> internalFunctions;
        std::unordered_map<std::string, Node *> FunctionBodies; // This is temporary, it only has to store data till the cache is created.
    };

    size_t getSizeOfType(size_t t, const std::vector<CachedStruct> &structs);
}