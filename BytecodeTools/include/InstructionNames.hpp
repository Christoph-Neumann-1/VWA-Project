#pragma once
//This files contains mappings from bytecode to assembly commands and back. This is used to avoid duplication of strings and to make sure both compiler and decompiler use
//the exact same names

#include <unordered_map>
#include <string>
#include <array>
#include <Bytecode.hpp>

namespace vwa::bc
{
    std::unordered_map<const std::string &, BcInstruction> strInstrMap; //This is supposed to be generated automatically and refer to the strings in the array below
    std::array<std::string, LastInstr> instrStrMap;
}