#pragma once
#include <vector>
#include <string>
#include <variant>
#include <optional>
#include <unordered_map>

namespace vwa
{
    struct Token
    {
        enum class Type
        {
            id,

            string_literal,
            char_literal,
            int_literal,
            long_literal,
            float_literal,
            double_literal,
            // TODO: unsigned_literal,
            rbrace,
            lbrace,
            rbracket,
            lbracket,
            rparen,
            lparen,

            comma,
            semicolon,
            colon,

            dot,
            plus,
            minus,
            asterix,
            slash,
            modulo,
            double_asterix,
            ampersand,
            exclamation,
            double_ampersand,
            double_pipe,
            eq,
            neq,
            gt,
            lt,
            gte,
            lte,
            assign,
            if_,
            else_,
            while_,
            for_,
            break_,
            continue_,
            return_,
            struct_,
            import_,
            export_,
            func_,
            let,
            mut_,

            eof,

            // Not implemented yet
            arrow_,
            const_expr,
            thick_arrow_, // Possibly for lambdas
            double_colon, // Namespace
            elipsis,      // Variadic function and for loops
            size_of,
            new_,
            delete_,
            tailrec,
        };
        Type type;
        std::variant<std::monostate, std::string, char, int32_t, int64_t, float, double, bool> value = {};
        uint64_t line = 0;
        // The function below is for debug purposes only, it doesn't not convert to the original string.
        std::string toString() const;
    };

    inline const std::unordered_map<std::string, Token::Type> reservedIdentifiers{
        {"if", Token::Type::if_},
        {"else", Token::Type::else_},
        {"while", Token::Type::while_},
        {"for", Token::Type::for_},
        {"break", Token::Type::break_},
        {"continue", Token::Type::continue_},
        {"return", Token::Type::return_},
        {"struct", Token::Type::struct_},
        {"import", Token::Type::import_},
        {"export", Token::Type::export_},
        {"constexpr", Token::Type::const_expr},
        {"let", Token::Type::let},
        {"mut", Token::Type::mut_},
        {"new", Token::Type::new_},
        {"delete", Token::Type::delete_},
        {"tailrec", Token::Type::tailrec},
        {"sizeof", Token::Type::size_of},
        {"func", Token::Type::func_},
    };

    std::optional<std::vector<Token>> tokenize(const std::string &input);
}