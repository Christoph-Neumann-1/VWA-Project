#pragma once
#include <Bytecode.hpp>
#include <Linker.hpp>

namespace vwa
{
    constexpr size_t stackSize = 2147483648;
    class VM
    {
        struct Stack
        {
            uint8_t *data;
            uint8_t *top; // Lets just hope this never overflows
            Stack()
            {
                data = new uint8_t[stackSize];
                top = data;
            }
            ~Stack()
            {
                delete[] data;
            }
        };
        Stack stack;
        int64_t exec(const bc::BcToken *bc);
    };
    // I might use something like this if I ever want to implement a c interface
    // struct VM_Wrapper
    // {
    //     VM_Wrapper(VM *vm) : vm(vm) {}
    //     VM *vm;
    //     uint64_t (*execute)(VM *vm, bc::BcInstruction *instruction)
    // };
}
