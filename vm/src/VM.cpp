#include <VM.hpp>
#include <Linker.hpp>
#include <cstring>
#include <cmath>

namespace vwa
{

    // TODO: proper error handling
    VM::ExitCode VM::exec(const bc::BcToken *bc, uint8_t *basePtr)
    {
        if (!basePtr)
        {
            basePtr = stack.data;
            setupStack();
        }
        using namespace bc;
        for (;;)
        {
            switch (bc->instruction)
            {
            case CallFunc:
                // TODO: find most efficient way to do this
                return {CallFunc, ExitCode::HelperInstructionInCode};
            // No cleanup is necessary, this is supposed to terminate the program.
            case Exit:
                // TODO: refactor this to also return the exit value on stack allowing for returning different types.
                // This is just an idea.
                return {stack.pop<int64_t>(), ExitCode::Exit};

            case JumpRel:
                bc += *reinterpret_cast<const int64_t *>((bc + 1));
                continue;
            case JumpRelIfFalse:
            {
                auto where = *reinterpret_cast<const int64_t *>((bc + 1));
                bc = stack.pop<int64_t>() ? bc + 9 : bc + where;
                continue;
            }
            case JumpRelIfTrue:
            {
                auto where = *reinterpret_cast<const int64_t *>((bc + 1));
                bc = stack.pop<int64_t>() ? bc + where : bc + 9;
                continue;
            }
            case JumpFuncRel:
            {
                uint64_t nArgs = *reinterpret_cast<const uint64_t *>((bc + 9));
                memmove(stack.top - nArgs + 2 * sizeof(uint64_t), stack.top - nArgs, nArgs);
                *reinterpret_cast<const uint8_t **>(stack.top - nArgs) = basePtr;
                *reinterpret_cast<const BcToken **>(stack.top - nArgs + 8) = bc + 17;
                basePtr = stack.top - nArgs;
                stack.top += 16;
                bc += *reinterpret_cast<const int64_t *>((bc + 1));
                continue;
            }
            case JumpFuncAbs:
            {
                uint64_t nArgs = *reinterpret_cast<const uint64_t *>((bc + 9));
                memmove(stack.top - nArgs + 16, stack.top - nArgs, nArgs);
                *reinterpret_cast<const uint8_t **>(stack.top - nArgs) = basePtr;
                *reinterpret_cast<const BcToken **>(stack.top - nArgs + 8) = bc + 17;
                basePtr = stack.top - nArgs;
                stack.top += 16;
                bc = *reinterpret_cast<const BcToken **>(const_cast<bc::BcToken *>(bc + 1));
                continue;
            }
            case JumpFFI:
            {
                Linker::FFIFunc func = *reinterpret_cast<const Linker::FFIFunc *>((bc + 1));
                func(this, stack.top);
                bc += 17;
                continue;
            }
            case Return:
            {
                uint64_t nBytes = *reinterpret_cast<const uint64_t *>((bc + 1));
                auto oldBase = basePtr;
                bc = *reinterpret_cast<const BcToken **>(basePtr + 8);
                basePtr = *reinterpret_cast<uint8_t **>(basePtr);
                memmove(oldBase, stack.top - nBytes, nBytes);
                stack.top = oldBase + nBytes;
                if (!bc)
                    // This is not the true exit code. Since a function may return anything,
                    // it is not viable to store the object in the return value.
                    // The caller is expected to deal with the exit code, e.g by using language
                    // features for threading or by directly accessing the vm stack.
                    // The 0 is just set to indicate everything was terminated normally
                    return {0, ExitCode::Success};
                continue;
            }
            case Push:
                stack.top += *reinterpret_cast<const uint64_t *>((bc + 1));
                bc += 9;
                continue;
            case Pop:
                stack.top -= *reinterpret_cast<const uint64_t *>((bc + 1));
                bc += 9;
                continue;
            case Push64:
                // I don't care about the type of data, I just expect it to work. If it causes problems, I will use std::bitcast
                stack.push(*reinterpret_cast<const int64_t *>((bc + 1)));
                bc += 9;
                continue;
            case Push8:
                stack.push(*reinterpret_cast<const int8_t *>((bc + 1)));
                bc += 2;
                continue;
            case Dup:
            {
                auto n = *reinterpret_cast<const uint64_t *>((bc + 1));
                auto ptr = stack.top - n;
                memmove(stack.top, ptr, n);
                stack.top += n;
                bc += 9;
                continue;
            }
            case ReadAbs:
            {
                auto addr = *reinterpret_cast<const void **>(const_cast<bc::BcToken *>(bc + 1));
                auto size = *reinterpret_cast<const uint64_t *>((bc + 9));
                memcpy(stack.top, addr, size);
                stack.top += size;
                bc += 17;
                continue;
            }
            case WriteAbs:
            {
                auto addr = *reinterpret_cast<void **>(const_cast<bc::BcToken *>(bc + 1));
                auto size = *reinterpret_cast<const uint64_t *>((bc + 9));
                memcpy(addr, stack.top - size, size);
                stack.top -= size;
                bc += 17;
                continue;
            }
            // It might be easier to implement this using offsets from the first valid element on the stack, but it would involve additional computation, or negative offsets,
            // which I don't want to deal with right now.
            case ReadRel:
            {
                auto addr = *reinterpret_cast<const intptr_t *>((bc + 1));
                auto size = *reinterpret_cast<const uint64_t *>((bc + 9));
                memcpy(stack.top, reinterpret_cast<void *>(basePtr + addr), size);
                stack.top += size;
                bc += 17;
                continue;
            }
            case WriteRel:
            {
                auto addr = *reinterpret_cast<const intptr_t *>((bc + 1));
                auto size = *reinterpret_cast<const uint64_t *>((bc + 9));
                memcpy(reinterpret_cast<void *>(basePtr + addr), stack.top - size, size);
                stack.top -= size;
                bc += 17;
                continue;
            }
            case AbsOf:
                stack.push<uint64_t>(reinterpret_cast<intptr_t>(basePtr) + *reinterpret_cast<const uint64_t *>((bc + 1)));
                bc += 9;
                continue;
            case AddI:
                stack.push(stack.pop<int64_t>() + stack.pop<int64_t>());
                ++bc;
                continue;
            case SubI:
            {
                auto rhs = stack.pop<int64_t>();
                auto lhs = stack.pop<int64_t>();
                stack.push(lhs - rhs);
                ++bc;
                continue;
            }
            case MulI:
                stack.push(stack.pop<int64_t>() * stack.pop<int64_t>());
                ++bc;
                continue;
            case DivI:
            {
                auto rhs = stack.pop<int64_t>();
                auto lhs = stack.pop<int64_t>();
                stack.push(lhs / rhs);
                ++bc;
                continue;
            }
            case ModI:
            {
                auto rhs = stack.pop<int64_t>();
                auto lhs = stack.pop<int64_t>();
                stack.push(lhs % rhs);
                ++bc;
                continue;
            }
            case PowerI:
            {
                auto rhs = stack.pop<int64_t>();
                auto lhs = stack.pop<int64_t>();
                int64_t result = 1;
                if (rhs >= 0)
                    for (int64_t i = 0; i < rhs; ++i)
                        result *= lhs;
                else
                {
                    // TODO: I am pretty sure this is broken in some way.
                    for (int64_t i = 0; i < -rhs; ++i)
                        result /= lhs;
                }
                stack.push(result);
                ++bc;
                continue;
            }
            case AddF:
            {
                auto rhs = stack.pop<double>();
                auto lhs = stack.pop<double>();
                stack.push(lhs + rhs);
                ++bc;
                continue;
            }
            case SubF:
            {
                auto rhs = stack.pop<double>();
                auto lhs = stack.pop<double>();
                stack.push(lhs - rhs);
                ++bc;
                continue;
            }
            case MulF:
            {
                auto rhs = stack.pop<double>();
                auto lhs = stack.pop<double>();
                stack.push(lhs * rhs);
                ++bc;
                continue;
            }
            case DivF:
            {
                auto rhs = stack.pop<double>();
                auto lhs = stack.pop<double>();
                stack.push(lhs / rhs);
                ++bc;
                continue;
            }
            case PowerF:
            {
                auto rhs = stack.pop<double>();
                auto lhs = stack.pop<double>();
                // FIXME: Add error handling
                stack.push(pow(lhs, rhs));
                ++bc;
                continue;
            }
            // TODO: decide wether characters are to be signed or unsigned
            case FtoI:
                stack.push(static_cast<int64_t>(stack.pop<double>()));
                ++bc;
                continue;
            case FtoC:
                stack.push(static_cast<char>(stack.pop<double>()));
                ++bc;
                continue;
            case ItoC:
                stack.push(static_cast<char>(stack.pop<int64_t>()));
                ++bc;
                continue;
            case ItoF:
                stack.push(static_cast<double>(stack.pop<int64_t>()));
                ++bc;
                continue;
            case CtoI:
                stack.push(static_cast<int64_t>(stack.pop<char>()));
                ++bc;
                continue;
            case CtoF:
                stack.push(static_cast<double>(stack.pop<char>()));
                ++bc;
                continue;
            // While it would make sense for booleans to be one byte in size, it is easier to
            // have them be ints like everything else and support characters just for C interop.
            // TODO: make sure there are ways to use character types in user code.
            case And:
                stack.push<int64_t>(stack.pop<int64_t>() && stack.pop<int64_t>());
                ++bc;
                continue;
            case Or:
                stack.push<int64_t>(stack.pop<int64_t>() || stack.pop<int64_t>());
                ++bc;
                continue;
            case Not:
                stack.push<int64_t>(!stack.pop<int64_t>());
                ++bc;
                continue;
            case GreaterThanF:
            {
                auto rhs = stack.pop<double>();
                auto lhs = stack.pop<double>();
                stack.push<int64_t>(lhs > rhs);
                ++bc;
                continue;
            }
            case GreaterThanI:
            {
                auto rhs = stack.pop<int64_t>();
                auto lhs = stack.pop<int64_t>();
                stack.push<int64_t>(lhs > rhs);
                ++bc;
                continue;
            }
            case LessThanF:
            {
                auto rhs = stack.pop<double>();
                auto lhs = stack.pop<double>();
                stack.push<int64_t>(lhs < rhs);
                ++bc;
                continue;
            }
            case LessThanI:
            {
                auto rhs = stack.pop<int64_t>();
                auto lhs = stack.pop<int64_t>();
                stack.push<int64_t>(lhs < rhs);
                ++bc;
                continue;
            }
            case GreaterThanOrEqualF:
            {
                auto rhs = stack.pop<double>();
                auto lhs = stack.pop<double>();
                stack.push<int64_t>(lhs >= rhs);
                ++bc;
                continue;
            }
            case GreaterThanOrEqualI:
            {
                auto rhs = stack.pop<int64_t>();
                auto lhs = stack.pop<int64_t>();
                stack.push<int64_t>(lhs >= rhs);
                ++bc;
                continue;
            }
            case LessThanOrEqualF:
            {
                auto rhs = stack.pop<double>();
                auto lhs = stack.pop<double>();
                stack.push<int64_t>(lhs <= rhs);
                ++bc;
                continue;
            }
            case LessThanOrEqualI:
            {
                auto rhs = stack.pop<int64_t>();
                auto lhs = stack.pop<int64_t>();
                stack.push<int64_t>(lhs <= rhs);
                ++bc;
                continue;
            }
            case EqualF:
            {
                auto rhs = stack.pop<double>();
                auto lhs = stack.pop<double>();
                stack.push<int64_t>(lhs == rhs);
                ++bc;
                continue;
            }
            case EqualI:
            {
                auto rhs = stack.pop<int64_t>();
                auto lhs = stack.pop<int64_t>();
                stack.push<int64_t>(lhs == rhs);
                ++bc;
                continue;
            }
            case NotEqualF:
            {
                auto rhs = stack.pop<double>();
                auto lhs = stack.pop<double>();
                stack.push<int64_t>(lhs != rhs);
                ++bc;
                continue;
            }
            case NotEqualI:
            {
                auto rhs = stack.pop<int64_t>();
                auto lhs = stack.pop<int64_t>();
                stack.push<int64_t>(lhs != rhs);
                ++bc;
                continue;
            }
            }
        }
    }
}