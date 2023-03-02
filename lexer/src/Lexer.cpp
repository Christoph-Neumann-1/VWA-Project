#include <Lexer.hpp>
#include <exception>
#include <cctype>
#include <algorithm>

namespace vwa
{

    // TODO: add to preprocessor also
    struct WindowsException : std::exception
    {
        const char *what() const noexcept override
        {
            return "It appears that you are running Windows, or using some weird windows features like carriage returns.\n"
                   "This is unsupported, please remove Windows from your PC and try again.";
        }
    };

    static std::optional<Token> parseNumber(Preprocessor::File::charIterator &it)
    {
        auto &str = it.it->str;
        auto &begin = it.pos;
        size_t firstNonDigit = str.size();
        bool dotFound = false;
        for (size_t i = begin; i < str.size(); i++)
        {
            if (str[i] == '.')
            {
                if (dotFound)
                    return std::nullopt;
                dotFound = true;
            }
            else if (!std::isdigit(str[i]))
            {
                firstNonDigit = i;
                break;
            }
        }
        // Error in calling code, this should never happen UNLESS I wanna allow .5
        if (firstNonDigit == begin)
            return std::nullopt;
        // This too, should not occur
        if (firstNonDigit == begin + 1 && str[0] == '.')
            return std::nullopt;
        size_t len;

        if (dotFound)
        {
            Token t{Token::Type::float_literal};
            t.value = std::stod(str.data() + begin, &len);
            begin += len - 1;
            return t;
        }
        else
        {
            Token t{Token::Type::int_literal};
            t.value = std::stol(str.data() + begin, &len);
            begin += len - 1;
            return t;
        }
    }

    std::optional<char> handleEscapedChar(char c)
    {
        switch (c)
        {
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case '\\':
            return '\\';
        case '\'':
            return '\'';
        case '\"':
            return '\"';
        default:
            return std::nullopt;
        }
    }

    std::optional<std::vector<Token>> tokenize(Preprocessor::File &input, Logger &log)
    {
        log << Logger::Info << "Beginning tokenization\n";
        log << Logger::Warning << "Missing logs for tokenization\n";
        Preprocessor::File::charIterator it = input.begin();
        std::vector<Token> tokens;
        while (1)
        {
            if (it.it == input.end())
            {
                tokens.push_back({Token::Type::eof});
                return tokens;
            }
            switch (*it)
            {
                // tokens.push_back(Token{Token::Type::eof, {}});
                // return tokens;
            case '\r':
                throw WindowsException();
            case '\0':
            case '\n':
            case ' ':
            case '\t':
                ++it;
                continue;
#pragma region brackets
            case '(':
                tokens.push_back({Token::Type::lparen, {}});
                break;
            case ')':
                tokens.push_back({Token::Type::rparen, {}});
                break;
            case '{':
                tokens.push_back({Token::Type::lbrace, {}});
                break;
            case '}':
                tokens.push_back({Token::Type::rbrace, {}});
                break;
            case '[':
                tokens.push_back({Token::Type::lbracket, {}});
                break;
            case ']':
                tokens.push_back({Token::Type::rbracket, {}});
                break;
#pragma endregion brackets
#pragma region operators
            case ',':
                tokens.push_back({Token::Type::comma, {}});
                break;
            case ';':
                tokens.push_back({Token::Type::semicolon, {}});
                break;
            case ':':
                if (*(it + 1) == ':')
                {
                    tokens.push_back({Token::Type::double_colon, {}});
                    ++it;
                }
                else
                {
                    tokens.push_back({Token::Type::colon, {}});
                }
                break;
            case '.':
                if (!std::isdigit(*(it + 1)))
                {
                    tokens.push_back({Token::Type::dot, {}});
                    break;
                }
                [[fallthrough]];
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            {
                if (auto ret = parseNumber(it); ret)
                    tokens.emplace_back(ret.value());
                else
                {
                    printf("Error: Unknown number at line %i.\n", it.it->line);
                    return {};
                }
            }
            break;
            case '-':
                switch (*(it + 1))
                {
                case '>':
                    tokens.push_back({Token::Type::arrow_, {}});
                    ++it;
                    break;
                default:
                    tokens.push_back({Token::Type::minus, {}});
                    break;
                }
                break;
            case '+':
                tokens.push_back({Token::Type::plus, {}});
                break;
            case '*':
                switch (*(it + 1))
                {
                    {
                    case '*':
                        tokens.push_back({Token::Type::double_asterix, {}});
                        ++it;
                        break;
                    default:
                        tokens.push_back({Token::Type::asterix, {}});
                        break;
                    }
                }
                break;
            case '/':
                tokens.push_back({Token::Type::slash, {}});
                break;
            case '%':
                tokens.push_back({Token::Type::modulo, {}});
                break;
            case '&':
                switch (*(it + 1))
                {
                    {
                    case '&':
                        tokens.push_back({Token::Type::double_ampersand, {}});
                        ++it;
                        break;
                    default:
                        tokens.push_back({Token::Type::ampersand, {}});
                        break;
                    }
                }
                break;
            case '!':
                switch (*(it + 1))
                {
                    {
                    case '=':
                        tokens.push_back({Token::Type::neq, {}});
                        ++it;
                        break;
                    default:
                        tokens.push_back({Token::Type::exclamation, {}});
                        break;
                    }
                }
                break;
            case '|':
                switch (*(it + 1))
                {
                case '|':
                    tokens.push_back({Token::Type::double_pipe, {}});
                    ++it;
                    break;
                default:
                    printf("Tokenizing failed at line %i. Reason: expected '|' but got '%c'\n", it.it->line, *it);
                    return {};
                }
                break;
            case '=':
                switch (*(it + 1))
                {
                case '=':
                    tokens.push_back({Token::Type::eq, {}});
                    ++it;
                    break;
                default:
                    tokens.push_back({Token::Type::assign, {}});
                    break;
                }
                break;
            case '>':
                switch (*(it + 1))
                {
                case '=':
                    tokens.push_back({Token::Type::gte, {}});
                    ++it;
                    break;
                default:
                    tokens.push_back({Token::Type::gt, {}});
                    break;
                }
                break;
            case '<':
                switch (*(it + 1))
                {
                case '=':
                    tokens.push_back({Token::Type::lte, {}});
                    ++it;
                    break;
                default:
                    tokens.push_back({Token::Type::lt, {}});
                    break;
                }
                break;
#pragma endregion operators
            case '"':
            {
                auto &current = it.pos;
                auto begin = current + 1;
                // Multi line strings may be supported in the future
                while (!(*(it + 1) == '"' && *it != '\\'))
                    ++it;
                std::string str;
                str.reserve(current - begin + 1);
                for (auto i = begin; i <= current; ++i)
                {
                    switch (it.it->str[i])
                    {
                    case '\\':
                        if (auto escaped = handleEscapedChar(it.it->str[++i]); escaped)
                        {
                            str += escaped.value();
                            break;
                        }
                        printf("Tokenizing failed at line %i. Reason: unknown escape sequence '\\%c'\n", it.it->line, it.it->str[i]);
                        return {};
                    default:
                        str += it.it->str[i];
                        break;
                    }
                }
                str.shrink_to_fit();
                tokens.push_back({Token::Type::string_literal, std::move(str)});
                ++it;
            }
            break;
            case '\'':
                if (it.it->str.length() <= it.pos + 2)
                    printf("Tokenizing failed at line %i. Reason: reached end of file", it.it->line);
                switch (*(it + 1))
                {
                case '\\':
                    if (it.it->str.length() <= it.pos + 3)
                    {
                        printf("Tokenizing failed at line %i. Reason: reached end of file", it.it->line);
                    }

                    if (auto escaped = handleEscapedChar(*(it + 2)); escaped)
                    {
                        tokens.push_back({Token::Type::int_literal, int64_t{escaped.value()}});
                        it += 3;
                    }
                    printf("Tokenizing failed at line %i. Reason: unknown escape sequence '\\%c'\n", it.it->line, *(it + 2));
                    return {};
                    break;
                default:
                    tokens.push_back({Token::Type::int_literal, static_cast<int64_t>(*(it + 1))});
                    it += 2;
                    break;
                }
                if (*it != '\'')
                {
                    printf("Tokenizing failed at line %i. Reason: expected '\'' but got '%c'. Unclosed character literal\n", it.it->line, *it);
                    return {};
                }
                break;
            default:
            {
                auto begin = it.pos;
                auto current = it.pos;
                if (*it != '_' && !isalpha(*it))
                {
                    printf("Tokenizing failed at line %i. Reason: expected identifier but got '%c'\n", it.it->line, it.it->str[current]);
                    return {};
                }
                while (isalnum(it.it->str[current]) || it.it->str[current] == '_')
                    ++current;
                if (begin == current)
                {
                    printf("Tokenizing failed at line %i.Nothing found\n", it.it->line);
                    return {};
                }
                auto str = it.it->str.substr(begin, current - begin);
                if (auto i = reservedIdentifiers.find(str); i != reservedIdentifiers.end())
                    tokens.push_back({i->second, {}});
                else
                {
                    if (str == "true")
                        tokens.push_back({Token::Type::int_literal, true});
                    else if (str == "false")
                        tokens.push_back({Token::Type::int_literal, false});
                    else
                        tokens.push_back({Token::Type::id, std::move(str)});
                }
                it.pos = current - 1;
            }
            }
            tokens.back().line = it.it->line;
            ++it;
        }
    }

    std::string Token::toString() const
    {
        switch (type)
        {
        case Token::Type::id:
            return "id(" + std::get<std::string>(value) + ")";
        case Token::Type::int_literal:
            return "int(" + std::to_string(std::get<int64_t>(value)) + ")";
        case Token::Type::float_literal:
            return "float(" + std::to_string(std::get<double>(value)) + ")";
        case Token::Type::string_literal:
            return "string(" + std::get<std::string>(value) + ")";
        case Token::Type::lparen:
            return "lparen";
        case Token::Type::rparen:
            return "rparen";
        case Token::Type::lbracket:
            return "lbracket";
        case Token::Type::rbracket:
            return "rbracket";
        case Token::Type::lbrace:
            return "lbrace";
        case Token::Type::rbrace:
            return "rbrace";
        case Token::Type::comma:
            return "comma";
        case Token::Type::semicolon:
            return "semicolon";
        case Token::Type::colon:
            return "colon";
        case Token::Type::dot:
            return "dot";
        case Token::Type::arrow_:
            return "arrow";
        case Token::Type::eq:
            return "eq";
        case Token::Type::assign:
            return "assign";
        case Token::Type::plus:
            return "plus";
        case Token::Type::minus:
            return "minus";
        case Token::Type::asterix:
            return "asterix";
        case Token::Type::double_asterix:
            return "double_asterix";
        case Token::Type::slash:
            return "slash";
        case Token::Type::modulo:
            return "modulo";
        case Token::Type::ampersand:
            return "ampersand";
        case Token::Type::double_ampersand:
            return "double_ampersand";
        case Token::Type::double_pipe:
            return "double_pipe";
        case Token::Type::exclamation:
            return "exclamation";
        case Token::Type::neq:
            return "neq";
        case Token::Type::lt:
            return "lt";
        case Token::Type::gt:
            return "gt";
        case Token::Type::lte:
            return "lte";
        case Token::Type::gte:
            return "gte";
        default:
        {
            if (auto it = std::find_if(reservedIdentifiers.begin(), reservedIdentifiers.end(), [this](const std::pair<std::string, Type> &p)
                                       { return p.second == type; });
                it != reservedIdentifiers.end())
                return it->first;
            return "undefined";
        }
        }
    }
}