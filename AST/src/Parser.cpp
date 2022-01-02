#include <Parser.hpp>

namespace vwa
{

    /*
     * Function layout:
     Id: name
     Type: return type
     Parameters:
     DeclVar: name,type, value:bool:mutable
     value:bool:constexpr
     */
    [[nodiscard]] static Pass1Result::Function parseFunction(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Pass1Result::Struct parseStruct(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static std::vector<Pass1Result::Parameter> parseParameterList(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Pass1Result::Type parseType(const std::vector<Token> &tokens, size_t &pos);

    Pass1Result buildTree(const std::vector<Token> &tokens)
    {
        size_t i = 0;
        bool exportSymbol = false;
        bool constexprSymbol = false;
        Pass1Result result;
        while (true)
        {
            if (i >= tokens.size())
                throw std::runtime_error("Unexpected end of file");
            switch (tokens[i].type)
            {
            case Token::Type::eof:
                return result;
            case Token::Type::func_:
            {
                auto func = parseFunction(tokens, i);
                func.exported = exportSymbol;
                func.constexpr_ = constexprSymbol;
                constexprSymbol = false;
                exportSymbol = false;
                result.functions.emplace(func.name, std::move(func));
                break;
            }
            case Token::Type::struct_:
            {
                if (constexprSymbol)
                    throw std::runtime_error("Structs cannot be constexpr");
                auto struct_ = parseStruct(tokens, i);
                struct_.exported = exportSymbol;
                exportSymbol = false;
                result.structs.emplace(struct_.name, std::move(struct_));
                break;
            }
            case Token::Type::import_:
            {
                if (constexprSymbol)
                    throw std::runtime_error("Imports cannot be constexpr");
                if (tokens[i + 1].type != Token::Type::string_literal)
                    throw std::runtime_error("Expected string after import");
                result.imports.push_back({std::get<std::string>(tokens[i + 1].value), exportSymbol});
                exportSymbol = false;
                i += 2;
                break;
            }
            case Token::Type::export_:
            {
                if (constexprSymbol)
                    throw std::runtime_error("Expected func after constexpr, got export");
                exportSymbol = true;
                ++i;
                break;
            }
            case Token::Type::const_expr:
            {
                constexprSymbol = true;
                ++i;
                break;
            }
            default:
                throw std::runtime_error("Unexpected token");
            }
        }
    }

    [[nodiscard]] static Pass1Result::Struct parseStruct(const std::vector<Token> &tokens, size_t &pos)
    {
        Pass1Result::Struct result;
        if (tokens[++pos].type != Token::Type::id)
            throw std::runtime_error("Expected identifier after struct");
        result.name = std::get<std::string>(tokens[pos].value);
        if (tokens[++pos].type != Token::Type::lbrace)
            throw std::runtime_error("Expected { after struct");
        result.fields = parseParameterList(tokens, ++pos);
        if (tokens[pos++].type != Token::Type::rbrace)
            throw std::runtime_error("Expected } after struct");
        return result;
    }

    [[nodiscard]] static std::vector<Pass1Result::Parameter> parseParameterList(const std::vector<Token> &tokens, size_t &pos)
    {
        std::vector<Pass1Result::Parameter> result;
        while (1)
        {
            if (tokens[pos].type != Token::Type::mut_ && tokens[pos].type != Token::Type::id)
                break;
            Pass1Result::Parameter p;
            p.isMutable = tokens[pos].type == Token::Type::mut_;
            pos += p.isMutable;
            if (tokens[pos].type != Token::Type::id)
                throw std::runtime_error("Expected identifier parameter declaration");
            p.name = std::get<std::string>(tokens[pos].value);
            if (tokens[++pos].type != Token::Type::colon)
                throw std::runtime_error("Expected : after parameter name");
            if (tokens[++pos].type != Token::Type::id)
                throw std::runtime_error("Expected typename after :");
            p.type = parseType(tokens, pos);
            result.push_back(std::move(p));
            if (tokens[pos].type == Token::Type::comma)
                ++pos;
        }
        return result;
    }

    [[nodiscard]] static Pass1Result::Function parseFunction(const std::vector<Token> &tokens, size_t &pos)
    {
        return {};
    }

    // Assumes that the first token is a id
    [[nodiscard]] static Pass1Result::Type parseType(const std::vector<Token> &tokens, size_t &pos)
    {
        Pass1Result::Type result;
        result.name = std::get<std::string>(tokens[pos].value);
        for (++pos; tokens[pos].type == Token::Type::asterix || tokens[pos].type == Token::Type::double_asterix; ++pos)
            result.pointerDepth += tokens[pos].type == Token::Type::asterix ? 1 : 2;
        return result;
    }

}