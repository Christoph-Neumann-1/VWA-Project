/**
 * @file template.hpp
 * This file includes some definitons required by all modules, such as those for the basic datatypes,
 * necessary includes and the autorun hooks as well as error handling code(planned)
 */

// FIXME: namespace for these definitions, then a seperate one for the implemented module

// Required to jump back into bytecode or other code for that matter
#include <VM.hpp>
// Contains the definitions for entry/exit functions and such
#include <Linker.hpp>

#include <functional>

namespace vwa
{
    // FIXME: I should probably move these somewhere else and use them everywhere
    using vm_int = int64;
    using vm_float = double;
    using vm_str = char *;

// Makes stuff a lot easier
#define VM_STRUCT struct __attribute__((packed))

    extern std::vector<Linker::Symbol> exports; // This is defined in the generated code

    // These do things like add definitions to the
    void InternalLoad();
    void InternalLink(Linker &l);
    void InternalExit();

    extern std::vector<void (*)()> loadCb;
    extern std::vector<void (*)(Linker &)> linkCb;
    extern std::vector<void (*)()> exitCb;

#ifdef MODULE_IMPL
    // TODO callbacks
    Linker::Module *VM_ONLOAD()
    {
        // TODO: add definitions here, if necessary
        InternalLoad();
        for (auto cb : loadCb)
            cb();
    }

    void VM_ONLINK(Linker *linker)
    {
        // TODO: load symbol definitions in here.
        InternalLink();
        for (auto cb : linkCb)
            cb(*linker);
    }

    void VM_EXIT()
    {
        InternalExit();
        for (auto cb : exitCb)
            cb();
        // TODO: cleanup
    }
#endif
    // TODO: default error handlers? How bout adding handlers for missing functions and downloading code at runtime

    // Dummy class, does absolutely nothing. Passing in a function pointer should also work, but I like statement expressions
    class Autorun
    {
        __attribute__((always_inline)) Autorun(int) {}
    };
#define INTERNAL_UNIQUE_NAME(name) CONCAT(vwa_internal_##name, __COUNTER__)
#define CONCAT(a, b) CONCAT_IMPL(a, b)
#define CONCAT_IMPL(a, b) a##b

#define ON_LOAD(f)                   \
    namespace                        \
    {                                \
        Autorun INTERNAL_UNIQUE_NAME \
        {                            \
            ({ loadCb.pushback(f); 0 });                   \
        }                            \
    }

#define ON_LINK(f)                   \
    namespace                        \
    {                                \
        Autorun INTERNAL_UNIQUE_NAME \
        {                            \
            ({ linkCb.pushback(f); 0 });                   \
        }                            \
    }

#define ON_EXIT(f)                   \
    namespace                        \
    {                                \
        Autorun INTERNAL_UNIQUE_NAME \
        {                            \
            ({ exitCb.pushback(f); 0 });                   \
        }                            \
    }

 
