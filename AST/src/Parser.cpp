#include <Parser.hpp>

namespace vwa
{
    // TODO: allow statements where expressions are expected
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
    [[nodiscard]] static Node::VarType parseType(const std::vector<Token> &tokens, size_t &pos);

    [[nodiscard]] static Node parseStatement(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parseBlock(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parseFunctionCall(Node func, const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parseExpression(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parseVarDeclaration(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parseIf(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parseWhile(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parseFor(const std::vector<Token> &tokens, size_t &pos);

    Pass1Result
    buildTree(const std::vector<Token> &tokens)
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
                result.functions.emplace_back(std::move(func));
                break;
            }
            case Token::Type::struct_:
            {
                if (constexprSymbol)
                    throw std::runtime_error("Structs cannot be constexpr");
                auto struct_ = parseStruct(tokens, i);
                struct_.exported = exportSymbol;
                exportSymbol = false;
                result.structs.emplace_back(std::move(struct_));
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
        Pass1Result::Function result;
        if (tokens[++pos].type != Token::Type::id)
            throw std::runtime_error("Expected identifier after func");
        result.name = std::get<std::string>(tokens[pos].value);
        if (tokens[++pos].type != Token::Type::lparen)
            throw std::runtime_error("Expected ( after func");
        result.parameters = parseParameterList(tokens, ++pos);
        if (tokens[pos++].type != Token::Type::rparen)
            throw std::runtime_error("Expected ) after func");
        if (tokens[pos].type == Token::Type::arrow_)
        {
            ++pos;
            if (tokens[pos].type != Token::Type::id)
                throw std::runtime_error("Expected identifier after ->");
            result.returnType = parseType(tokens, pos);
        }
        else
            result.returnType = {"void", 0};
        result.body = parseStatement(tokens, pos);
        return result;
    }

    // Assumes that the first token is a id
    [[nodiscard]] static Node::VarType parseType(const std::vector<Token> &tokens, size_t &pos)
    {
        Node::VarType result;
        result.name = std::get<std::string>(tokens[pos].value);
        for (++pos; tokens[pos].type == Token::Type::asterix || tokens[pos].type == Token::Type::double_asterix; ++pos)
            result.pointerDepth += tokens[pos].type == Token::Type::asterix ? 1 : 2;
        return result;
    }

    [[nodiscard]] static Node parseStatement(const std::vector<Token> &tokens, size_t &pos)
    {
        switch (tokens[pos].type)
        {
        case Token::Type::lbrace:
            return parseBlock(tokens, pos);
        case Token::Type::let:
            return parseVarDeclaration(tokens, pos);
        case Token::Type::if_:
            return parseIf(tokens, pos);
        case Token::Type::while_:
            return parseWhile(tokens, pos);
        case Token::Type::for_:
            return parseFor(tokens, pos);
        case Token::Type::return_:
        {
            if (tokens[pos + 1].type == Token::Type::semicolon)
            {
                auto line = tokens[pos].line;
                pos += 2;
                return Node{Node::Type::Return, {}, {}, line};
            }
            else
            {
                auto line = tokens[pos].line;
                auto expr = parseExpression(tokens, ++pos);
                if (tokens[pos++].type != Token::Type::semicolon)
                    throw std::runtime_error("Expected ; after return");
                return Node{Node::Type::Return, {}, {std::move(expr)}, line};
            }
        }
        case Token::Type::break_:
        {
            auto line = tokens[pos].line;
            if (tokens[pos + 1].type != Token::Type::semicolon)
                throw std::runtime_error("Expected ; after break");
            pos += 2;
            return Node{Node::Type::Break, {}, {}, line};
        }
        case Token::Type::continue_:
        {
            auto line = tokens[pos].line;
            if (tokens[pos + 1].type != Token::Type::semicolon)
                throw std::runtime_error("Expected ; after continue");
            pos += 2;
            return Node{Node::Type::Continue, {}, {}, line};
        }
        default:
            // Assume an expression like function call, or the lhs of assign. If it evaluates to something we discard it later.
            {
                auto expr = parseExpression(tokens, pos);
                if (tokens[pos].type == Token::Type::assign)
                {
                    ++pos;
                    auto line = expr.line;
                    Node result{Node::Type::Assign, {}, {std::move(expr), parseExpression(tokens, pos)}, line};
                    if (tokens[pos++].type != Token::Type::semicolon)
                        throw std::runtime_error("Expected ; after assign");
                    return result;
                }
                if (tokens[pos++].type != Token::Type::semicolon)
                    throw std::runtime_error("Expected ; after expression");
                return expr;
            }
        }
    }
    [[nodiscard]] static Node parseBlock(const std::vector<Token> &tokens, size_t &pos)
    {
        Node result{.type = Node::Type::Block, .line = tokens[pos].line};
        for (++pos; tokens[pos].type != Token::Type::rbrace;)
        {
            if (tokens[pos].type == Token::Type::eof)
                throw std::runtime_error("Unexpected end of file");
            result.children.push_back(parseStatement(tokens, pos));
        }
        ++pos;
        return result;
    }

    [[nodiscard]] static Node parseFunctionCall(Node func, const std::vector<Token> &tokens, size_t &pos)
    {
        Node result{Node::Type::CallFunc, {}, {func}, func.line};
        if (tokens[pos].type != Token::Type::lparen)
            throw std::runtime_error("Expected ( after function call");
        //FIXME: where am I handling commas?
        for (++pos; tokens[pos].type != Token::Type::rparen;)
        {
            if (tokens[pos].type == Token::Type::eof)
                throw std::runtime_error("Unexpected end of file");
            result.children.push_back(parseExpression(tokens, pos));
            if (tokens[pos].type == Token::Type::rparen)
                break;
            else
                ++pos;
        }
        ++pos;
        return result;
    }

    [[nodiscard]] static Node parseVarDeclaration(const std::vector<Token> &tokens, size_t &pos)
    {
        Node result{Node::Type::DeclareVar, {}, {}, tokens[pos++].line};
        if (tokens[pos].type != Token::Type::id && tokens[pos].type != Token::Type::mut_)
            throw std::runtime_error("Expected identifier or mut after let");
        bool isMutable = tokens[pos].type == Token::Type::mut_;
        pos += isMutable;
        if (isMutable && tokens[pos].type != Token::Type::id)
            throw std::runtime_error("Expected identifier after mut");
        result.children.push_back(Node{Node::Type::Variable, std::get<std::string>(tokens[pos].value), {}, tokens[pos].line});
        if (tokens[++pos].type != Token::Type::colon)
            throw std::runtime_error("Expected : after variable name");
        if (tokens[++pos].type != Token::Type::id)
            throw std::runtime_error("Expected typename after :");
        auto line = tokens[pos].line;
        result.children.push_back({Node::Type::Type, parseType(tokens, pos), {}, line});
        result.children.push_back({Node::Type::Flag, isMutable, {}, line});
        if (tokens[pos].type == Token::Type::assign)
        {
            ++pos;
            result.children.push_back(parseExpression(tokens, pos)); // TODO: translate into assign in next pass
        }
        if (tokens[pos++].type != Token::Type::semicolon)
            throw std::runtime_error("Expected ; after variable declaration");
        return result;
    }

    [[nodiscard]] static Node parseIf(const std::vector<Token> &tokens, size_t &pos)
    {
        auto line = tokens[pos].line;
        if (tokens[++pos].type != Token::Type::lparen)
            throw std::runtime_error("Expected ( after if");
        auto cond = parseExpression(tokens, ++pos);
        if (tokens[pos].type != Token::Type::rparen)
            throw std::runtime_error("Expected ) after if condition");
        auto statement = parseStatement(tokens, ++pos);
        if (tokens[pos].type == Token::Type::else_)
        {
            auto elseStatement = parseStatement(tokens, ++pos);
            return {Node::Type::If, {}, {std::move(cond), std::move(statement), std::move(elseStatement)}, line};
        }
        return {Node::Type::If, {}, {std::move(cond), std::move(statement)}, line};
    }
    [[nodiscard]] static Node parseWhile(const std::vector<Token> &tokens, size_t &pos)
    {
        auto line = tokens[pos].line;
        if (tokens[++pos].type != Token::Type::lparen)
            throw std::runtime_error("Expected ( after while");
        auto cond = parseExpression(tokens, ++pos);
        if (tokens[pos].type != Token::Type::rparen)
            throw std::runtime_error("Expected ) after while condition");
        return {Node::Type::While, {}, {std::move(cond), parseStatement(tokens, ++pos)}, line};
    }
    [[nodiscard]] static Node parseFor(const std::vector<Token> &tokens, size_t &pos)
    {
        auto line = tokens[pos].line;
        if (tokens[++pos].type != Token::Type::lparen)
            throw std::runtime_error("Expected ( after for");
        auto init = parseStatement(tokens, pos);
        auto cond = parseExpression(tokens, pos);
        if (tokens[pos].type != Token::Type::rparen)
            throw std::runtime_error("Expected ) after for condition");
        auto step = parseStatement(tokens, ++pos);
        auto body = parseStatement(tokens, ++pos);
        return {Node::Type::For, {}, {std::move(init), std::move(cond), std::move(step), std::move(body)}, line};
    }

    [[nodiscard]] static Node parseLogical(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parseComparison(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parseAddSub(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parseMulDiv(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parsePower(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parseUnary(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parsePrimary(const std::vector<Token> &tokens, size_t &pos);
    [[nodiscard]] static Node parseMemberAccess(const std::vector<Token> &tokens, size_t &pos);

    [[nodiscard]] static Node parseExpression(const std::vector<Token> &tokens, size_t &pos)
    {
        return parseLogical(tokens, pos);
    }

    [[nodiscard]] static Node parseLogical(const std::vector<Token> &tokens, size_t &pos)
    {
        auto result = parseComparison(tokens, pos);
        while (tokens[pos].type == Token::Type::double_ampersand || tokens[pos].type == Token::Type::double_pipe)
        {
            auto line = tokens[pos].line;
            result = {tokens[pos].type == Token::Type::double_ampersand ? Node::Type::And : Node::Type::Or, {}, {std::move(result), parseComparison(tokens, ++pos)}, line};
        }
        return result;
    }

    [[nodiscard]] static Node parseComparison(const std::vector<Token> &tokens, size_t &pos)
    {
        auto result = parseAddSub(tokens, pos);
        while (1)
        {
            Node::Type t;
            switch (tokens[pos].type)
            {
            case Token::Type::lt:
                t = Node::Type::LessThan;
                break;
            case Token::Type::gt:
                t = Node::Type::GreaterThan;
                break;
            case Token::Type::lte:
                t = Node::Type::LessThanEqual;
                break;
            case Token::Type::gte:
                t = Node::Type::GreaterThanEqual;
                break;
            case Token::Type::eq:
                t = Node::Type::Equal;
                break;
            case Token::Type::neq:
                t = Node::Type::NotEqual;
                break;
            default:
                return result;
            }
            auto line = tokens[pos].line;
            result = {t, {}, {std::move(result), parseAddSub(tokens, ++pos)}, line};
        }
    }

    [[nodiscard]] static Node parseAddSub(const std::vector<Token> &tokens, size_t &pos)
    {
        auto result = parseMulDiv(tokens, pos);
        while (tokens[pos].type == Token::Type::plus || tokens[pos].type == Token::Type::minus)
        {
            auto line = tokens[pos].line;
            result = {tokens[pos].type == Token::Type::plus ? Node::Type::Plus : Node::Type::Minus, {}, {std::move(result), parseMulDiv(tokens, ++pos)}, line};
        }
        return result;
    }

    [[nodiscard]] static Node parseMulDiv(const std::vector<Token> &tokens, size_t &pos)
    {
        auto rhs = parsePower(tokens, pos);
        while (tokens[pos].type == Token::Type::asterix || tokens[pos].type == Token::Type::slash)
        {
            auto line = tokens[pos].line;
            rhs = {tokens[pos].type == Token::Type::asterix ? Node::Type::Multiply : Node::Type::Divide, {}, {std::move(rhs), parsePower(tokens, ++pos)}, line};
        }
        return rhs;
    }

    [[nodiscard]] static Node parsePower(const std::vector<Token> &tokens, size_t &pos)
    {
        auto lhs = parseUnary(tokens, pos);
        if (tokens[pos].type == Token::Type::double_asterix)
        {
            auto line = tokens[pos].line;
            return {Node::Type::Power, {}, {std::move(lhs), parsePower(tokens, ++pos)}, line};
        }
        return lhs;
    }

    [[nodiscard]] static Node parseUnary(const std::vector<Token> &tokens, size_t &pos)
    {
        switch (tokens[pos].type)
        {
        case Token::Type::plus:
            return parseUnary(tokens, ++pos);
            // TODO: make lexer handle negative numbers
        case Token::Type::minus:
        {
            auto line = tokens[pos].line;
            return {Node::Type::UnaryMinus, {}, {parseUnary(tokens, ++pos)}, line};
        }
        case Token::Type::exclamation:
        {
            auto line = tokens[pos].line;
            return {Node::Type::Not, {}, {parseUnary(tokens, ++pos)}, line};
        }
        case Token::Type::asterix:
        {
            auto line = tokens[pos].line;
            return {Node::Type::Dereference, {}, {parseUnary(tokens, ++pos)}, line};
        }
        case Token::Type::ampersand:
        {
            auto line = tokens[pos].line;
            return {Node::Type::AddressOf, {}, {parseUnary(tokens, ++pos)}, line};
        }
        default:
            return parsePrimary(tokens, pos);
        }
    }
    // FIXME: I forgot testing parantheses
    [[nodiscard]] static Node parsePrimary(const std::vector<Token> &tokens, size_t &pos)
    {
        switch (tokens[pos].type)
        {
        case Token::Type::int_literal:
        {
            auto line = tokens[pos].line;
            return {Node::Type::LiteralI, {std::get<int64_t>(tokens[pos++].value)}, {}, line};
        }
        case Token::Type::float_literal:
        {
            auto line = tokens[pos].line;
            return {Node::Type::LiteralF, {std::get<double>(tokens[pos++].value)}, {}, line};
        }
        // TODO: make sure to treat strings as char*
        case Token::Type::string_literal:
        {
            auto line = tokens[pos].line;
            return {Node::Type::LiteralS, {std::get<std::string>(tokens[pos++].value)}, {}, line};
        }
        case Token::Type::id:
            return parseMemberAccess(tokens, pos);
        case Token::Type::lparen:
        {
            auto result = parseExpression(tokens, ++pos);
            if (tokens[pos++].type != Token::Type::rparen)
            {
                throw std::runtime_error("Expected ')'");
            }
            return result;
        }
        default:
            throw std::runtime_error("Unexpected token");
        }
    }
    // TODO: Check if it is faster to assume a variable first
    [[nodiscard]] static Node parseMemberAccess(const std::vector<Token> &tokens, size_t &pos)
    {
        Node root{Node::Type::MemberAccess, {}, {}, tokens[pos].line};
        while (1)
        {
            if (tokens[pos].type == Token::Type::lparen)
            {
                if (root.children.size() == 1)
                    root = std::move(root.children[0]);
                root = {Node::Type::MemberAccess, {}, {parseFunctionCall(std::move(root), tokens, pos)}, tokens[pos].line};
            }
            else if (tokens[pos].type == Token::Type::dot)
            {
                ++pos;
            }
            else if (tokens[pos].type == Token::Type::id)
            {
                // TODO: cache this later on
                root.children.push_back(Node{Node::Type::Variable, std::get<std::string>(tokens[pos++].value), {}, root.line});
            }
            else
            {
                if (root.children.size() == 1)
                    return std::move(root.children[0]);
                return root;
            }
        }
    }
}