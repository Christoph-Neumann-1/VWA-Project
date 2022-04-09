#pragma once
// This files contains mappings from bytecode to assembly commands and back. This is used to avoid duplication of strings and to make sure both compiler and decompiler use
// the exact same names

#include <unordered_map>
#include <string>
#include <array>
#include <Bytecode.hpp>

namespace vwa::bc
{
    constexpr std::array<const char *, LastInstr> instrStrMap{
        "CallFunc",
        "Break",
        "Continue",
        "Exit",
        "JumpRel",
        "JumpRelIfFalse",
        "JumpRelIfTrue",
        "JumpFuncRel",
        "JumpFPtr",
        "JumpFuncAbs",
        "JumpFFI",
        "Return",
        "Push",
        "Pop",
        "Push64",
        "Push8",
        "Dup",
        "ReadAbs",
        "WriteAbs",
        "ReadRel",
        "WriteRel",
        "ReadMember",
        "AbsOf",
        "AbsOfConst",
        "AddI",
        "SubI",
        "MulI",
        "DivI",
        "ModI",
        "PowerI",
        "AddF",
        "SubF",
        "MulF",
        "DivF",
        "PowerF",
        "NegF",
        "FtoI",
        "FtoC",
        "ItoC",
        "ItoF",
        "CtoI",
        "CtoF",
        "And",
        "Or",
        "Not",
        "GreaterThanF",
        "LessThanF",
        "GreaterThanOrEqualF",
        "LessThanOrEqualF",
        "EqualF",
        "NotEqualF",
        "GreaterThanI",
        "LessThanI",
        "GreaterThanOrEqualI",
        "LessThanOrEqualI",
        "EqualI",
        "NotEqualI",
    };
    extern std::unordered_map<std::string, BcInstruction> strInstrMap;
}