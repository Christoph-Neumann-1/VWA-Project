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
        return ({ auto t = dotFound ? ({bool isDouble = firstNonDigit == str.size() ? false : str[firstNonDigit] == 'f';Token t{isDouble ? Token::Type::double_literal : Token::Type::float_literal, isDouble ? std::stod(str.data()+begin, &len) : std::stof(str.data()+begin, &len)}; begin-=!isDouble;t; })
                                        : ({bool isLong = firstNonDigit == str.size() ? false : str[firstNonDigit] == 'l';Token t{isLong ? Token::Type::long_literal : Token::Type::int_literal, isLong ? std::stol(str.data()+begin, &len) : std::stoi(str.data()+begin, &len)}; begin-=!isLong;t; });
            begin += len;
            t; });
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

    std::optional<std::vector<Token>> tokenize(std::string input)
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
                break;
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
                    tokens.push_back({Token::Type::id, std::move(str)});
                --current;
            }
            }
            tokens.back().line = lineCounter;
            ++current;
        }
    }
}