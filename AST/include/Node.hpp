#pragma once
#include <variant>
#include <string>
#include <vector>
#include <memory>
#include <Linker.hpp>
namespace vwa
{


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
            Cast,
            TypePun,
        };
        Type type = Type::Unassigned;
        // struct VarType
        // {
        //     Identifier name;
        //     uint32_t pointerDepth = 0;
        //     bool operator==(const VarType &other) const
        //     {
        //         return name == other.name && pointerDepth == other.pointerDepth;
        //     }
        // };
        // struct VarTypeCached
        // {
        //     size_t index;
        //     uint32_t pointerDepth;
        // };
        std::variant<std::monostate, std::string, Identifier, char, int32_t, int64_t, float, double, bool, Linker::VarType, Linker::Cache::CachedType> value = {};
        std::vector<Node> children{};
        uint64_t line = 0;
        // TODO: toString
    };
}
