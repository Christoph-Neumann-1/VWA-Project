#pragma once
#include <variant>
#include <string>
#include <vector>
#include <memory>

namespace vwa
{

    struct Node
    {
        enum class Type
        {
            Function,
            Import,
            Struct,

            DeclareVar,
            Type,
            Variable,
            CallFunc,
            LiteralI,
            LiteralL,
            LiteralF,
            LiteralD,
            LiteralC,
            LiteralS,
            Block,
            If,
            Else,
            While,
            For,
            Break,
            Continue,
            Return,

            Plus,
            Minus,
            Multiply,
            Divide,
            Modulo,
            Power,
            And,
            Or,
            Not,
            Equal,
            NotEqual,
            GreaterThan,
            LessThan,
            GreaterThanEqual,
            LessThanEqual,
            Assign,
            AddressOf,
            Dereference,
            DotOperator,
        };

        Type type;
        std::variant<std::monostate, std::string, char, int32_t, int64_t, float, double, bool> value = {};
        std::vector<std::unique_ptr<Node>> children;
        uint64_t line = 0;
        // TODO: toString
    };
}