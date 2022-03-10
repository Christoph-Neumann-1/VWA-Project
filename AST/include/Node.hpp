#pragma once
#include <variant>
#include <string>
#include <vector>
#include <memory>
namespace vwa
{

    struct Identifier
    {
        std::string name;
        std::string module_ = ""; // TODO: add a constructor

        bool operator==(const Identifier &other) const
        {
            return name == other.name && module_ == other.module_;
        }
    };

    struct Node
    {
        enum class Type
        {
            Unassigned,

            DeclareVar,
            DiscardValue,
            Type,
            Flag, // Bool value for example to store whether a function is constexpr
            Variable,
            CallFunc,
            LiteralI,
            LiteralF,
            LiteralS, // This should get translated into an int pointer later
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
            SizeOf,
        };
        Type type = Type::Unassigned;
        struct VarType
        {
            Identifier name;
            uint32_t pointerDepth = 0;
        };
        struct VarTypeCached
        {
            size_t index;
            uint32_t pointerDepth;
        };
        std::variant<std::monostate, std::string, Identifier, char, int32_t, int64_t, float, double, bool, VarType, VarTypeCached> value = {};
        std::vector<Node> children{};
        uint64_t line = 0;
        // TODO: toString
    };
}

namespace std
{
    template <>
    struct hash<vwa::Identifier>
    {
        // I really hope this works
        size_t operator()(const vwa::Identifier &id) const
        {
            return hash<string>()(id.name) ^ hash<string>()(id.module_);
        }
    };
}