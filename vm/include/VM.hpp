#pragma once
#include <Bytecode.hpp>
#include <Linker.hpp>

namespace vwa
{
    constexpr size_t stackSize = 2147483648;
    class VM
    {
        // TODO: write at for stack and read arg for vm
        struct Stack
        {
            uint8_t *data;
            uint8_t *top; // Lets just hope this never overflows. Considering how slow the language is anyways, it would be a good idea to check for overflow, but I will fix that as soon as I implement virtual adress spaces.
            Stack()
            {
                data = new uint8_t[stackSize];
                top = data;
            }
            ~Stack()
            {
                delete[] data;
            }
            template <typename T>
            void push(const T value)
            {
                *reinterpret_cast<T *>(top) = value;
                top += sizeof(T);
            }

            template <typename T>
            T pop()
            {
                top -= sizeof(T);
                return *reinterpret_cast<T *>(top);
            }
        };

    public:
        struct ExitCode
        {
            // Doesn't have to be a failure, this can also indicate that exit was called and the program
            // should terminate.
            enum Message : uint64_t
            {
                Success, // Continue execution
                Exit,    // Not really and error, someone just called exit.

                HelperInstructionInCode, // e.g CallFunc. Code is in exitCode.
                InvalidInstruction,
            };
            int64_t exitCode; // If this is the main function, this is the return value.
            Message statusCode;
        };

        Stack stack;
        // Begin execution on the adress provieded. This assumes, that all necessary arguments are already on the stack.
        // Base pointer is necessary for passing arguments, the function to know where to return to.
        // In the case of nullptr, the base is assumed to be the start of the stack. This also calls setup stack for you.
        ExitCode exec(const bc::BcToken *bc, uint8_t *basePtr = nullptr);
        // This sets up the stack to return back to the caller of exec afterwards, it achieves this by setting the return adress, as well as the base pointer to null.
        // Use the adress before this as the base pointer. After calling this function you may push arguments to the stack.
        void setupStack()
        {
            stack.push<uint64_t>(0);
            stack.push<uint64_t>(0);
        }
    };
    // I might use something like this if I ever want to implement a c interface
    // struct VM_Wrapper
    // {
    //     VM_Wrapper(VM *vm) : vm(vm) {}
    //     VM *vm;
    //     uint64_t (*execute)(VM *vm, bc::BcInstruction *instruction)
    // };
}
