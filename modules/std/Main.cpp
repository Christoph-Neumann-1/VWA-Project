#include <Linker.hpp>
#include <VM.hpp>

using namespace vwa;

Linker::Module mod;

void add(VM *vm, uint8_t *)
{
    vm->stack.push(vm->stack.pop<int64_t>() + vm->stack.pop<int64_t>());
}

void sayHello(VM *vm, uint8_t *)
{
    printf("Hello, world!\n");
}

//Don't call this more than once
//Returns all symbols and imports
extern "C" Linker::Module *MODULE_LOAD()
{
    mod.exportedSymbols.push_back({"add", Linker::Module::Symbol::Function{Linker::Module::Symbol::Function::Type::External, {.ffi = add}, {{"int", 0}, {"int", 0}}, {"int", 0}, false}});
    mod.exportedSymbols.push_back({"sayHello", Linker::Module::Symbol::Function{Linker::Module::Symbol::Function::Type::External, {.ffi = sayHello}, {}, {"void", 0}, false}});
    return &mod;
}

//Gets called after linking, in here function pointers are resolved
extern "C" void MODULE_SETUP()
{
}

//In case there are any cleanup functions
extern "C" void MODULE_UNLOAD()
{
}