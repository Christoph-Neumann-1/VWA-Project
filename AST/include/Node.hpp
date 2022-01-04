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
            Unassigned,

            Function,
            Import,
            Struct,

            DeclareVar,
            DiscardValue,
            Type,
            Variable,
            CallFunc,
            LiteralI,
            LiteralL,
            LiteralF,
            LiteralD,
            LiteralC,
            LiteralS,
            LiteralB,
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
            UnaryMinus,
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
            MemberAccess,
        };
        Type type = Type::Unassigned;
        struct VarType
        {
            std::string name;
            uint32_t pointerDepth = 0;
        };
        std::variant<std::monostate, std::string, char, int32_t, int64_t, float, double, bool, VarType> value = {};
        std::vector<Node> children{};
        uint64_t line = 0;
        // TODO: toString
    };
}