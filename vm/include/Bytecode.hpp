#pragma once
#include <cstdint>

// TODO: add a way to handle chars, either by adding the type, or by adding a function in the standard library
// TODO: consider adding noop

namespace vwa::bc
{
    // Possible optimization: remove infrequently used instructions like CtoD and replace them with CtoI ItoD to reduce instruction count.
    // It might also help to remove smaller integer types from the instruction set, since their performance advantage is negligible compared to the overhead of the interpreter.
    enum BcInstruction : uint8_t
    {
        CallFunc, // Placeholer generated by the compiler. The linker replaces this with JumpFunc or JumpFFI. It is not relevant to the compiler if this is a ffi or internal function.
        //Both of these are just helper instructions, they are almost immediately replaced by a relative jump.
        Break,
        Continue,
        // Callfunc is on top,so that the compiler can optimize the switch statement further.
        Exit, // Terminates the program. Args: i32 (exit code)

        // Control flow
        // Relative jumps are relative to the current instruction.
        // They are used for all module internal functions.
        // Some functions only have a relative variant, since there is no need to perform a conditional jump into another module.
        JumpRel,        // Jump to adress and continue execution. Args: i64 address(passed in bytecode)
        JumpRelIfFalse, // Jump to adress if the last value is false. Args: bool condition,i64 address(passed in bytecode)
        JumpRelIfTrue,  // Jump to the address if the argument is true. Args:  bool condition,i64 address(passed in bytecode)
        JumpFuncRel,    // Jump to the address, while setting the base pointer and return address. Args: i64 address, u64 nBytes for args, all passed in bytecode
        JumpFPtr,       // Either calls a ffi or normal function. Args: see above
        // Btw JumpFuncRel is also abused by the compiler to store indices for internal functions if their position is unknown.
        JumpFuncAbs, // Call a function from an other module(or maybe a function pointer) Same as JumpFuncRel, but the address is absolute.
        JumpFFI,     // Jump to the ffi function at the given address. Args: u64 address. No stack pointer needs to be set. nBytes is not required for this instruction, but the 8 bytes
        // are still reserved to allow both JumpFunc and JumpFFI to be exchanged by the linker
        Return, // Restore base pointer and jump to the return address on the stack. Args: u64 nBytes for rval
        // TODO: get absolute address of func
        //  Stack manipulation
        Push, // Increment the stack pointer. This does not allow for intialization of the value. Args: u64 nBytes
        Pop,  // Decrement the stack pointer. Args: u64 nBytes
        // If performance is bad I might also add instructions for pushing data from the bytecode.

        // For some common operations I added specific instructions.
        //  They do what you think they do.
        Push64,
        Push8,

        Dup, // Args: u64 nBytes

        // Data manipulation
        // Adress for this is on the stack.
        ReadAbs,  // Reads data at an absolute address. Args: u64 address, u64 nBytes
        WriteAbs, // Writes data at an absolute address. Args: u64 address, u64 nBytes
        // Args for this  are all in the bytecode.
        ReadRel,    // Reads data relative to the base pointer. Used for local variables and access to the constant pool. Args: i64 offset, u64 nBytes If the signed offset causes problems I will add a bool indicating whether to add or subtract.
        WriteRel,   // Writes data relative to the base pointer. Used for local variables and access to the constant pool. Args: i64 offset, u64 nBytes
        ReadMember, // Reads struct member, Args: u64 struct size, u64 member offset, u64 nBytes
        // TODO: stack pointer relative read/write
        AbsOf, // Convert relative adress given in bc to absolute adress.

        // Character types don't get their own instructions, they are just converted to integers.
        //  TODO: unsigned types
        //  TODO: consider negation instructions
        AddI,
        SubI,
        MulI,
        DivI,
        ModI,
        PowerI,

        AddF,
        SubF,
        MulF,
        DivF,
        PowerF,
        NegF,

        // Conversion
        FtoI,
        FtoC,

        ItoC,
        ItoF,

        CtoI,
        CtoF,

        And, // Args: 2 ints
        Or,  // Args: 2 ints
        Not, // Args: 1 ints

        // Comparison
        GreaterThanF,
        LessThanF,
        GreaterThanOrEqualF,
        LessThanOrEqualF,
        EqualF,
        NotEqualF,

        // TODO Integer types don't need to have the orEqual instructions since they can be rewritten using other instructions.
        GreaterThanI,
        LessThanI,
        GreaterThanOrEqualI,
        LessThanOrEqualI,
        EqualI,
        NotEqualI,
        LastInstr, // This is not a real instruction, it is just meant as a place holder for when an array of opcodes is necessary as cpp doesn't provide a way to get the size of an enum
    };
    union BcToken
    {
        uint8_t value;
        BcInstruction instruction;
    };
};