#pragma once
#include <Node.hpp>
#include <unordered_map>
#include <Lexer.hpp>

namespace vwa
{
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
            bool exported;
        };

        std::unordered_map<std::string, Function> functions;
        std::unordered_map<std::string, Struct> structs;
        std::vector<Import> imports;
    };

    Pass1Result buildTree(const std::vector<Token> &tokens);
}
