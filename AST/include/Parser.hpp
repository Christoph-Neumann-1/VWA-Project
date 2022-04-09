#pragma once
#include <Node.hpp>
#include <unordered_map>
#include <Lexer.hpp>

namespace vwa
{
    // TODO: there is no reason this is not using the symbol struct from the linker (This may require significant refactoring)
    // TODO: to string method
    struct Pass1Result
    {
        struct Parameter
        {
            std::string name;
            Node::VarType type;
            bool isMutable;
        };

        struct Struct
        {
            std::string name;
            std::vector<Parameter> fields;
            bool exported;
        };

        struct Function
        {
            std::string name;
            Node::VarType returnType;
            std::vector<Parameter> parameters;
            Node body;
            bool constexpr_;
            bool exported;
        };

        struct Import
        {
            std::string name;
        };

        std::vector<Function> functions;
        std::vector<Struct> structs;
        std::vector<Import> imports;
    };

    Pass1Result buildTree(const std::vector<Token> &tokens);
}
