#include <Lexer.hpp>
#include <exception>
#include <cctype>

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

    static std::optional<Token> parseNumber(const std::string &str, size_t &begin)
    {
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
        // Error in calling code, this should never happen
        if (firstNonDigit == begin)
            return std::nullopt;
        // This too, should not occur
        if (firstNonDigit == begin + 1 && str[0] == '.')
            return std::nullopt;
        size_t len;

        switch (dotFound)
        {
        case true:
        {
            bool isDouble = firstNonDigit == str.size() ? false : str[firstNonDigit] == 'd';
            Token t{isDouble ? Token::Type::double_literal : Token::Type::float_literal};

            switch (isDouble)
            {
            case true:
                t.value = std::stod(str.data() + begin, &len);
                break;
            case false:
                t.value = std::stof(str.data() + begin, &len);
                break;
            }
            begin += len - !isDouble;
            return t;
        }
        case false:
        {
            bool isLong = firstNonDigit == str.size() ? false : str[firstNonDigit] == 'l';
            Token t{isLong ? Token::Type::long_literal : Token::Type::int_literal};
            switch (isLong)
            {
            case true:
                t.value = std::stol(str.data() + begin, &len);
                break;
            case false:
                t.value = std::stoi(str.data() + begin, &len);
                break;
            }
            begin += len - !isLong;
            return t;
        }
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

    std::optional<std::vector<Token>> tokenize(const std::string &input)
    {
        size_t current = 0;
        uint64_t lineCounter = 1;
        std::vector<Token> tokens;
        while (1)
        {
            switch (input[current])
            {
            case '\0':
                tokens.push_back(Token{Token::Type::eof, {}});
                return tokens;
            case '\r':
                throw WindowsException();
            case '\n':
                ++lineCounter;
            case ' ':
            case '\t':
                ++current;
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
                tokens.push_back({Token::Type::colon, {}});
                break;
            case '.':
                if (!std::isdigit(input[current + 1]))
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
                if (auto ret = parseNumber(input, current); ret)
                    tokens.emplace_back(ret.value());
                else
                {
                    printf("Error: Unknown number at line %lu.\n", lineCounter);
                    return {};
                }
            }
            break;
            case '-':
                switch (input[current + 1])
                {
                case '>':
                    tokens.push_back({Token::Type::arrow_, {}});
                    ++current;
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
                switch (input[current + 1])
                {
                    {
                    case '*':
                        tokens.push_back({Token::Type::double_asterix, {}});
                        ++current;
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
                switch (input[current + 1])
                {
                    {
                    case '&':
                        tokens.push_back({Token::Type::double_ampersand, {}});
                        ++current;
                        break;
                    default:
                        tokens.push_back({Token::Type::ampersand, {}});
                        break;
                    }
                }
                break;
            case '!':
                switch (input[current + 1])
                {
                    {
                    case '=':
                        tokens.push_back({Token::Type::neq, {}});
                        ++current;
                        break;
                    default:
                        tokens.push_back({Token::Type::exclamation, {}});
                        break;
                    }
                }
                break;
            case '|':
                switch (input[current + 1])
                {
                case '|':
                    tokens.push_back({Token::Type::double_pipe, {}});
                    ++current;
                    break;
                default:
                    printf("Tokenizing failed at line %lu. Reason: expected '|' but got '%c'\n", lineCounter, input[current]);
                    return {};
                }
                break;
            case '=':
                switch (input[current + 1])
                {
                case '=':
                    tokens.push_back({Token::Type::eq, {}});
                    ++current;
                    break;
                default:
                    tokens.push_back({Token::Type::assign, {}});
                    break;
                }
                break;
            case '>':
                switch (input[current + 1])
                {
                case '=':
                    tokens.push_back({Token::Type::gte, {}});
                    ++current;
                    break;
                default:
                    tokens.push_back({Token::Type::gt, {}});
                    break;
                }
                break;
            case '<':
                switch (input[current + 1])
                {
                case '=':
                    tokens.push_back({Token::Type::lte, {}});
                    ++current;
                    break;
                default:
                    tokens.push_back({Token::Type::lt, {}});
                    break;
                }
                break;
#pragma endregion operators
            case '"':
            {
                auto begin = current + 1;
                // I am aware this allows for multi line strings. This is a feature, not a bug.
                while (input[current + 1] != '"' && input[current] != '\\')
                    ++current;
                std::string str;
                str.reserve(current - begin + 1);
                for (auto i = begin; i <= current; ++i)
                {
                    switch (input[i])
                    {
                    case '\\':
                        if (auto escaped = handleEscapedChar(input[++i]); escaped)
                        {
                            str += escaped.value();
                            break;
                        }
                        printf("Tokenizing failed at line %lu. Reason: unknown escape sequence '\\%c'\n", lineCounter, input[i]);
                        return {};
                    default:
                        str += input[i];
                        break;
                    }
                }
                str.shrink_to_fit();
                tokens.push_back({Token::Type::string_literal, std::move(str)});
                ++current;
            }
            break;
            case '\'':
                if (input.length() <= current + 2)
                    printf("Tokenizing failed at line %lu. Reason: reached end of file", lineCounter);
                switch (input[current + 1])
                {
                case '\\':
                    if (input.length() <= current + 3)
                    {
                        printf("Tokenizing failed at line %lu. Reason: reached end of file", lineCounter);
                    }

                    if (auto escaped = handleEscapedChar(input[current + 2]); escaped)
                    {
                        tokens.push_back({Token::Type::char_literal, escaped.value()});
                        current += 3;
                    }
                    printf("Tokenizing failed at line %lu. Reason: unknown escape sequence '\\%c'\n", lineCounter, input[current + 2]);
                    return {};
                    break;
                default:
                    tokens.push_back({Token::Type::char_literal, input[current + 1]});
                    current += 2;
                    break;
                }
                if (input[current] != '\'')
                {
                    printf("Tokenizing failed at line %lu. Reason: expected '\'' but got '%c'. Unclosed character literal\n", lineCounter, input[current]);
                    return {};
                }
                break;
            default:
            {
                auto begin = current;
                if (input[begin] != '_' && !isalpha(input[begin]))
                {
                    printf("Tokenizing failed at line %lu. Reason: expected identifier but got '%c'\n", lineCounter, input[begin]);
                    return {};
                }
                while (isalnum(input[current]) || input[current] == '_')
                    ++current;
                auto str = input.substr(begin, current - begin);
                if (auto i = reservedIdentifiers.find(str); i != reservedIdentifiers.end())
                    tokens.push_back({i->second, {}});
                else
                {
                    if (str == "true")
                        tokens.push_back({Token::Type::bool_literal, true});
                    else if (str == "false")
                        tokens.push_back({Token::Type::bool_literal, false});
                    else
                        tokens.push_back({Token::Type::id, std::move(str)});
                }
                --current;
            }
            }
            tokens.back().line = lineCounter;
            ++current;
        }
    }

    std::string Token::toString() const
    {
        switch (type)
        {
        case Token::Type::id:
            return "id(" + std::get<std::string>(value) + ")";
        case Token::Type::int_literal:
            return "int(" + std::to_string(std::get<int32_t>(value)) + ")";
        case Token::Type::long_literal:
            return "long(" + std::to_string(std::get<int64_t>(value)) + ")";
        case Token::Type::float_literal:
            return "float(" + std::to_string(std::get<float>(value)) + ")";
        case Token::Type::double_literal:
            return "double(" + std::to_string(std::get<double>(value)) + ")";
        case Token::Type::string_literal:
            return "string(" + std::get<std::string>(value) + ")";
        case Token::Type::char_literal:
            return "char(" + std::string(1, std::get<char>(value)) + ")";
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