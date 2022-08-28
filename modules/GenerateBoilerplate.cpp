#include <cstdint>
#include <vector>
#include <stdexcept>
#include <string>
#include <sstream>
#include <ctype.h>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <Linker.hpp>

namespace vwa::boilerplate
{
    using std::string;

    // TODO: this needs to go and be replaced by existing structures for compability
    struct Token
    {
        enum Type : uint8_t
        {
            asterix,
            namespace_operator,
            colon,
            openParen,
            closeParen,
            openCurly,
            closeCurly,
            arrow,
            comma,
            id,
        };
        Type type;
        string value{};
    };

    std::vector<vwa::boilerplate::Token> tokenize(std::istream &input)
    {
        std::vector<vwa::boilerplate::Token> tokens;
        int c;
        while ((c = input.get()) != EOF)
        {
            switch (c)
            {
            case ' ':
            case '\t':
            case '\n':
                break;
            case '\r':
                throw std::runtime_error("Please purge windows from you device and try again");
            case ':':
                switch (input.peek())
                {
                case ':':
                    input.get();
                    tokens.push_back({vwa::boilerplate::Token::namespace_operator});
                    break;
                default:
                    tokens.push_back({vwa::boilerplate::Token::colon});
                    break;
                }
                break;
            case '(':
                tokens.push_back({vwa::boilerplate::Token::openParen});
                break;
            case ')':
                tokens.push_back({vwa::boilerplate::Token::closeParen});
                break;
            case '{':
                tokens.push_back({vwa::boilerplate::Token::openCurly});
                break;
            case '}':
                tokens.push_back({vwa::boilerplate::Token::closeCurly});
                break;
            case '*':
                tokens.push_back({vwa::boilerplate::Token::asterix});
                break;
            case ',':
                tokens.push_back({vwa::boilerplate::Token::comma});
                break;
            case '-':
                if (input.peek() == '>')
                {
                    input.get();
                    tokens.push_back({vwa::boilerplate::Token::arrow});
                }
                else
                {
                    throw std::runtime_error("Unexpected character '-'");
                }
                break;
            default:
                if (std::isalpha(c))
                {
                    std::stringstream ss;
                    do
                        ss << static_cast<char>(c);
                    while (std::isalnum(c = input.get()) || c == '_');
                    input.unget();
                    tokens.push_back({vwa::boilerplate::Token::id, ss.str()});
                }
                else
                    throw std::runtime_error(string("Unexpected character '") + static_cast<char>(c) + "'");
            }
        }
        return tokens;
    }
    Linker::VarType parseType(const std::vector<vwa::boilerplate::Token> &tokens, size_t &pos)
    {
        if (pos >= tokens.size() || tokens[pos].type != vwa::boilerplate::Token::id)
            throw std::runtime_error("Expected type name");
        Linker::VarType result = {{tokens[pos++].value}};
        if (pos + 2 < tokens.size() && tokens[pos].type == vwa::boilerplate::Token::namespace_operator)
        {
            pos += 2;
            result.name.module = std::move(result.name.name);
            result.name.name = tokens[pos - 1].value;
        }
        for (; pos < tokens.size() && tokens[pos].type == vwa::boilerplate::Token::asterix; pos++)
            result.pointerDepth++;
        return result;
    }

    void processImport(Linker::Module &module, Linker::Symbol &symbol, Linker &linker)
    {
        if (symbol.name.module == module.name || symbol.name.module == "")
            throw std::runtime_error("Importing self is not allowed");
        auto &importedModule = linker.getModule(symbol.name.module);
    }

    std::unordered_map<Identifier, std::pair<char, const Linker::Symbol *>> processImports(const std::vector<vwa::Identifier> &imports, Linker::Module &thisMod, Linker &linker)
    {
        std::unordered_map<Identifier, std::pair<bool, Linker::Symbol *>> result;
        for (auto &import : imports)
        {
        }
    }

    string generateBoilerplate(std::istream &input, Linker &linker)
    {
        std::stringstream out;
        // Read the template file into the stringstream
        std::ifstream templateFile("template.hpp");
        out << templateFile.rdbuf();
        auto tokens = tokenize(input);
        size_t tokensSize = tokens.size();
        // yes, I could have stored the module name along with the token, but I choose not to, because it makes the lexer shorter
        if (tokensSize < 2 || tokens[0].type != vwa::boilerplate::Token::id || tokens[0].value != "module" || tokens[1].type != vwa::boilerplate::Token::id)
            throw std::runtime_error("Expected module at the top of the file");
        Linker::Module module;
        module.name = tokens[1].value;
        std::vector<Identifier> imports; // Grouped by module for later
        size_t pos = 2;
        if (pos + 1 < tokensSize && tokens[pos].type == vwa::boilerplate::Token::id && tokens[pos].value == "import" && tokens[pos + 1].type == vwa::boilerplate::Token::colon)
        {
            pos += 2;
            while (pos < tokensSize)
            {
                if (tokens[pos].type != vwa::boilerplate::Token::id)
                    throw std::runtime_error("Expected symbol name");
                auto &str = tokens[pos].value;
                if (str == "export" && pos + 1 < tokensSize && tokens[pos + 1].type == vwa::boilerplate::Token::colon)
                    break;
                if (!(pos + 2 < tokensSize && tokens[pos + 1].type == vwa::boilerplate::Token::namespace_operator && (tokens[pos + 2].type == vwa::boilerplate::Token::id || tokens[pos + 2].type == vwa::boilerplate::Token::asterix)))
                    throw std::runtime_error("Expected '::'");
                if (tokens[pos + 2].type == vwa::boilerplate::Token::asterix)
                {
                    auto &mod = linker.getModule(str);
                    for (auto &sym : mod.exports)
                        module.requiredSymbols.emplace_back(sym);
                }
                else
                    module.requiredSymbols.emplace_back(linker.getSymbol({tokens[pos + 2].value, str}));
                pos += 3;
            }
        }
        if (pos + 1 < tokensSize && tokens[pos].type == vwa::boilerplate::Token::id && tokens[pos].value == "export" && tokens[pos + 1].type == vwa::boilerplate::Token::colon)
        {
            pos += 2;
            // I am pretty certain, this is missing a loop somewhere
            // This steaming mess needs to be rewritten there are no off by one errors, there are off by three errors
            while (pos + 1 < tokensSize)
            {
                switch (tokens[pos + 1].type)
                {
                case vwa::boilerplate::Token::openCurly:
                {
                    Linker::Symbol symbol{{tokens[pos].value, module.name}, Linker::Symbol::Struct{}};
                    auto &data = std::get<Linker::Symbol::Struct>(symbol.data);
                    pos += 2;
                    while (pos < tokensSize && tokens[pos].type != vwa::boilerplate::Token::closeCurly)
                    {
                        if (tokens[pos].type != vwa::boilerplate::Token::id)
                            throw std::runtime_error("Expected symbol name");
                        auto &str = tokens[pos++].value;
                        if (pos >= tokens.size() || tokens[pos].type != vwa::boilerplate::Token::colon)
                            throw std::runtime_error("Expected ':'");
                        data.fields.push_back({str, parseType(tokens, ++pos)});
                        if (pos >= tokens.size())
                            throw std::runtime_error("Unexpected end of file");
                        switch (tokens[pos].type)
                        {
                        case Token::comma:
                            pos++;
                        case Token::closeCurly:
                            continue;
                        default:
                            throw std::runtime_error("Expected ',' or '}'");
                        }
                    }
                    if (pos >= tokens.size() || tokens[pos].type != Token::closeCurly)
                        throw std::runtime_error("Expected '}'");
                    pos++;
                    module.exports.push_back(std::move(symbol));
                    break;
                }
                case Token::openParen:
                {
                    Linker::Symbol symbol{{tokens[pos].value, module.name}, Linker::Symbol::Function{}};
                    auto &data = std::get<Linker::Symbol::Function>(symbol.data);
                    pos += 2;
                    while (pos < tokensSize && tokens[pos].type != Token::closeParen)
                    {
                        data.params.push_back({.type = parseType(tokens, pos)});
                        if (pos >= tokens.size())
                            throw std::runtime_error("Unexpected end of file");
                        if (tokens[pos].type == Token::comma)
                            pos++;
                        else if (tokens[pos].type != Token::closeParen)
                            throw std::runtime_error("Expected ',' or ')'");
                    }
                    ++pos;
                    if (pos + 1 < tokensSize && tokens[pos].type == Token::arrow)
                        data.returnType = parseType(tokens, ++pos);
                    else
                        data.returnType = {{"void"}, 0};
                    module.exports.push_back(std::move(symbol));
                    break;
                }
                default:
                    throw std::runtime_error("Unexpected token");
                }
            }
        }

        out << "namespace " << module.name<<"\n{";

        

        //TODO: hashmap of ids to cpp tokens
        out << '}';
        return out.str();
    }
}
int main()
{
    std::ifstream input("Samplein");
    vwa::Linker linker;
    std::cout << vwa::boilerplate::generateBoilerplate(input, linker);
    return 0;
}