#pragma once

#include <Compiler.hpp>
namespace vwa
{

    // static constexpr size_t Linker::Cache::reservedIndices = Linker::Cache::reservedIndicies;

    // TODO: remove
    //  To make live easier for myself I decided that 2 types is good enough for now, if you are using this language you are clearly not worried about performance anyway
    //  enum PrimitiveTypes
    //  {
    //      Void,
    //      I64,
    //      F64,
    //      U8, // Char type for c interop. This isn\t actually used to store characters, the compiler translates it into a I64 instead.
    //  };

    // Used for storing the true location of variables
    // If I implement stack pointer free functions I might also store more information
    // FIXME
    struct Scope
    {
        // 32 bits ought to be enough for anybody, this is the stack we are talking about!
        uint32_t size;
        uint32_t offset;

        Scope(uint64_t size, uint64_t offset) : size(size), offset(offset) {}
        struct Variable
        {
            Linker::Cache::CachedType type;
            uint32_t offset;
        };
        // TODO: UUID for variables
        std::unordered_map<std::string, Variable>
            variables{};
    };

    // This is used by the compiler for fast look up of struct sizes.
    // Member names get replaced by indices while processing the function, they are then used to look up their offset.
    // Currently this doesn't help much since I am only doing a single pass, but once I start adding optimizations it will reduce the cost of lookups.
    // struct CachedStruct
    // {
    //     struct Member
    //     {
    //         size_t offset = -1;
    //         size_t type = -1; // Not actually an index, the first few values are reserved for primitive types
    //         uint32_t ptrDepth = -1;
    //     };
    //     size_t size = -1;
    //     std::vector<Member> members{};
    //     size_t refCount = 0;               // This is used to remove dependencies on structs which are not actually used in the module.
    //     Linker::Symbol *symbol{0}; // Used if the name or type of the struct needs to be retrieved again, like for error reporting.
    //     bool internal : 1;
    //     enum State
    //     {
    //         Uninitialized,
    //         Processing,
    //         Finished,
    //     };
    //     State state : 2 = Uninitialized;
    //     constexpr static size_t npos = -1;
    // };

    // This is used by the compiler to replace known functions with their adresses and remove unused functions.
    // struct CachedFunction
    // {
    //     struct Parameter
    //     {
    //         size_t type = -1, pointerDepth = -1;
    //     };
    //     // I did not want to waste addtional space on loop detection, so I just repurposed the unused space in booleans
    //     std::vector<Parameter> args{};
    //     Parameter returnType{};
    //     uint64_t refCount = 0; // TODO: implement
    //     const Linker::Symbol *symbol{0};
    //     union
    //     {
    //         Node *body;
    //         int64_t address; // After completely compiling this function the address is stored here.
    //     };
    //     bool internal;
    //     bool finished = false; // TODO: set for ffi
    // };

    // struct Cache
    // {
    //     enum Type
    //     {
    //         Struct,
    //         Function,
    //     };
    //     // TODO: figure out if this is even necessery, or at least try to remove type from here.
    //     std::unordered_map<Identifier, std::pair<size_t, Type>> map{};
    //     std::vector<CachedStruct> structs{};
    //     std::vector<CachedFunction> functions{};
    // };

    // struct ProcessingResult
    // {
    //     Linker::Module *module;
    //     std::vector<Linker::Symbol> internalStructs;
    //     std::vector<Linker::Symbol> internalFunctions;
    //     std::unordered_map<std::string, Node *> FunctionBodies; // This is temporary, it only has to store data till the cache is created. And this probably shouln't be a map.
    // };

    // size_t getSizeOfType(size_t t, size_t ptr, const std::vector<CachedStruct> &structs);
}