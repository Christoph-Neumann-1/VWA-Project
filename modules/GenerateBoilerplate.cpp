// TODO: use the normal parser for this

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
#include <unordered_set>

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
        [[maybe_unused]] auto &importedModule = linker.getModule(symbol.name.module);
    }

    std::unordered_map<Identifier, std::pair<char, const Linker::Symbol *>> processImports(const std::vector<vwa::Identifier> &imports, Linker::Module &thisMod, Linker &linker)
    {
        std::unordered_map<Identifier, std::pair<bool, Linker::Symbol *>> result;
        for (auto &import : imports)
        {
        }
        throw std::runtime_error("Unimplemented");
    }

    // Linker is assumed to be unused
    std::pair<string, Linker::Module &> generateBoilerplate(std::istream &input, Linker &linker, bool interfaceOnly)
    {
        std::stringstream out;
        // Read the template file into the stringstream
        // std::ifstream templateFile("template.hpp");
        // out << templateFile.rdbuf();
        auto tokens = tokenize(input);
        size_t tokensSize = tokens.size();
        // yes, I could have stored the module name along with the token, but I choose not to, because it makes the lexer shorter
        if (tokensSize < 2 || tokens[0].type != vwa::boilerplate::Token::id || tokens[0].value != "module" || tokens[1].type != vwa::boilerplate::Token::id)
            throw std::runtime_error("Expected module at the top of the file");
        Linker::Module module;
        module.name = tokens[1].value;
        // std::vector<Identifier> imports;
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

        decltype(auto) mod = *linker.provideModule(std::move(module));
        linker.fillMissingModuleNames(mod);
        if (interfaceOnly)
            return {{}, mod};
        mod.satisfyDependencies(linker);

        out << "#include <VM.hpp>\n"
            << "#include <Linker.hpp>\n"
            << "namespace vwa\n"
            << "{\n"
            << "using vm_int=int64_t;\n"
            << "using vm_float = double;\n"
            // << "using vm_str = int *;\n"
            << "using vm_char = char;\n" // TODO: the rest of the types
            << "}\n"
            << "#define VM_STRUCT struct __attribute__((packed))\n";

        enum Status : uint8_t
        {
            PROCESSING,
            DONE,
        };
        std::unordered_map<Identifier, Status> status;
        const auto isPrimitive = [](const Identifier &id)
        {
            return id.name == "int" || id.name == "float" || id.name == "char" || id.name == "void" || id.name == "string" || id.name == "function";
        };

        std::unordered_set<Identifier> requiredSymbols;
        auto requireSym = [&requiredSymbols, &mod](Identifier id)
        {
            if (!id.module.length())
                id.module = mod.name;
            requiredSymbols.emplace(std::move(id));
        };

        const auto processDeclare = [&](const Linker::Symbol &sym, auto &&self)
        {
            if (std::holds_alternative<Linker::Symbol::Function>(sym.data))
                return;
            if (auto it = status.find(sym.name); it != status.end())
                return;
            if (sym.name.module != mod.name)
                // requiredSymbols.emplace(sym.name);
                requireSym(sym.name);

            out
                << "namespace " << sym.name.module << "{\n"
                << "VM_STRUCT " << sym.name.name << ";\n"
                << "}\n";
            status[sym.name] = DONE;
            auto &s = std::get<Linker::Symbol::Struct>(sym.data);
            for (auto &mem : s.fields)
                if (!isPrimitive(mem.type.name))
                    self(linker.getSymbol(mem.type.name), self);
        };

        for (auto &s : mod.exports)
            processDeclare(s, processDeclare);

        for (auto &s : mod.requiredSymbols)
            processDeclare(*std::get<1>(s), processDeclare);

        status.clear();

        const auto typeMap = [&mod, &requireSym](const Identifier &name) -> std::string
        {
            static std::unordered_map<std::string, std::string> builtIns{
                {"int", "::vwa::vm_int"},
                {"float", "::vwa::vm_float"},
                {"string", "::vwa::vm_str"},
                {"char", "::vwa::vm_char"},
                {"void", "void"},
            };
            if (auto it = builtIns.find(name.name); it != builtIns.end())
                return it->second;
            if (name.module == mod.name || !name.module.length())
            {
                requireSym(name);
                return "::" + mod.name + "::" + name.name;
            }
            return "::" + name.module + "::" + name.name;
        };
        const auto typeToStr = [&typeMap](const Linker::VarType t)
        {
            std::string ret{typeMap(t.name)};
            ret.reserve(ret.size() + t.pointerDepth);
            ret.append(t.pointerDepth, '*');
            return ret;
        };
        const auto fieldToStr = [&typeMap](const Linker::Symbol::Field &f)
        {
            std::string ret{typeMap(f.type.name)};
            ret.reserve(ret.size() + f.type.pointerDepth + f.name.size() + 1);
            ret.append(f.type.pointerDepth, '*');
            if (f.name.length())
            {
                ret += ' ';
                ret += f.name;
            }
            return ret;
        };

        const auto processDefine = [&](const Linker::Symbol &sym, auto &&self)
        {
            if (std::holds_alternative<Linker::Symbol::Function>(sym.data))
                return;
            if (auto it = status.find(sym.name); it != status.end())
                if (it->second == DONE)
                    return;
                else
                    throw std::runtime_error("Failed to process");

            auto &state = status[sym.name];
            auto &s = std::get<Linker::Symbol::Struct>(sym.data);
            state = PROCESSING;
            for (auto &mem : s.fields)
                if (!mem.type.pointerDepth && !isPrimitive(mem.type.name))
                    self(linker.getSymbol(mem.type.name), self);
            out << "namespace " << sym.name.module << "{\n"
                << "VM_STRUCT " << sym.name.name << "\n"
                << "{\n";
            for (auto &mem : s.fields)
                out << fieldToStr(mem) << ";\n";
            out << "};\n}\n";
            state = DONE;
            for (auto &mem : s.fields)
                if (mem.type.pointerDepth && !isPrimitive(mem.type.name))
                    self(linker.getSymbol(mem.type.name), self);
        };

        for (auto &s : mod.exports)
            processDefine(s, processDefine);
        for (auto &s : mod.requiredSymbols)
            processDefine(*std::get<1>(s), processDefine);

        const auto emplaceParams = [&out, &fieldToStr](const auto &params)
        {
            out << '(';
            if (params.size())
            {
                for (size_t i = 0; i < params.size() - 1; i++)
                    out << fieldToStr(params[i]) << ',';
                out << fieldToStr(params.back());
            }
            out << ')';
        };

        out << "namespace " << mod.name << "\n{\n";
        for (auto &s : mod.exports)
        {
            if (!std::holds_alternative<Linker::Symbol::Function>(s.data))
                continue;
            auto &f = std::get<Linker::Symbol::Function>(s.data);
            out << typeToStr(f.returnType);
            out << ' ' << s.name.name;
            emplaceParams(f.params);
            out << ";\n";
        }
        out << "}\n";

        for (auto &s : mod.requiredSymbols)
        {
            auto &sym = *std::get<1>(s);
            // requiredSymbols.emplace(sym.name);
            requireSym(sym.name);
            if (!std::holds_alternative<Linker::Symbol::Function>(sym.data))
                continue;
            out << "namespace " << sym.name.module << "\n{\n";
            auto &f = std::get<Linker::Symbol::Function>(sym.data);
            out << typeToStr(f.returnType);
            out << "(*" << sym.name.name << ')';
            emplaceParams(f.params);
            out << ";\n}\n";
        }

        mod.requiredSymbols.clear();
        mod.requiredSymbols.reserve(requiredSymbols.size());
        for (auto &id : requiredSymbols)
            mod.requiredSymbols.push_back(linker.getSymbol(id));

        // TODO:DRY
        const auto emplaceStruct = [&out](auto &st)
        {
            out << "::vwa::Linker::Symbol::Struct{{";
            for (auto &field : st.fields)
                out << "{\"" << field.name << "\",{{\"" << field.type.name.name << "\",\"" << field.type.name.module << "\"}," << field.type.pointerDepth << "}},";
            out << "}}";
        };

        out << "#ifdef MODULE_IMPL\n"
            << "::vwa::Linker::Module InternalLoad(){\n"
            << "::vwa::Linker::Module module{\n"
            << ".name=\"" << mod.name << "\",\n"
            << ".requiredSymbols={\n";
        for (auto &s : mod.requiredSymbols)
        {
            auto &sym = std::get<0>(s);
            out << "::vwa::Linker::Symbol{{\"" << sym.name.name << "\",\"" << sym.name.module << "\"},";
            if (std::holds_alternative<Linker::Symbol::Function>(sym.data))
            {
                auto &f = std::get<Linker::Symbol::Function>(sym.data);
                out << "::vwa::Linker::Symbol::Function{.params={";
                for (auto &field : f.params)
                    out << "{.type={{\"" << field.type.name.name << "\",\"" << field.type.name.module << "\"}," << field.type.pointerDepth << "}},";
                out << "},.returnType={{\"" << f.returnType.name.name << "\",\""
                    << f.returnType.name.module << "\"}," << f.returnType.pointerDepth << "}}";
            }
            else
                emplaceStruct(std::get<Linker::Symbol::Struct>(sym.data));
            out << "},\n";
        }
        out << "},\n"
            << ".exports={\n";

        for (auto &s : mod.exports)
        {
            out << "::vwa::Linker::Symbol{{\"" << s.name.name << "\",\"" << mod.name << "\"},";
            if (std::holds_alternative<Linker::Symbol::Function>(s.data))
            {
                auto &f = std::get<Linker::Symbol::Function>(s.data);
                out << "::vwa::Linker::Symbol::Function{.type=::vwa::Linker::Symbol::Function::External,.params={";
                for (auto &field : f.params)
                    out << "{.type={{\"" << field.type.name.name << "\",\"" << field.type.name.module << "\"}," << field.type.pointerDepth << "}},";
                out << "},.returnType={{\"" << f.returnType.name.name << "\",\""
                    << f.returnType.name.module << "\"}," << f.returnType.pointerDepth << "},"
                    << ".ffi=[](::vwa::VM *vm){";
                size_t counter{};
                for (auto &field : f.params)
                {
                    field.name = "p" + std::to_string(counter++);
                    out << fieldToStr(field) << ';';
                }
                for (auto it = f.params.rbegin(); it != f.params.rend(); it++)
                {
                    out << it->name << "=vm->stack.pop<" << typeToStr(it->type) << ">();";
                    // out << "std::memcpy(&" << it->name << ',' << "vm->stack.top-sizeof(" << typeToStr(it->type) << "),"
                    //     << "sizeof(" << typeToStr(it->type) << "));vm->stack.top-=sizeof(" << typeToStr(it->type) << ");";
                }
                bool isVoid = f.returnType.name.name == "void"&&!f.returnType.pointerDepth;
                if (!isVoid)
                    out << "vm->stack.push(";
                out << "::" << s.name.module << "::" << s.name.name << "(";
                if (f.params.size())
                {
                    for (size_t i = 0; i < f.params.size() - 1; i++)
                        out << f.params[i].name << ',';
                    out << f.params.back().name;
                }
                if (!isVoid)
                    out << ')';
                out << ");}";

                out
                    << ",.ffi_direct=reinterpret_cast<void*>(::" << mod.name << "::" << s.name.name
                    << ")}";
            }
            else
                emplaceStruct(std::get<Linker::Symbol::Struct>(s.data));
            out << "},\n";
        }

        out << "}};\n"
            << "return module;\n"
            << "}\n"

            << "void InternalLink(::vwa::Linker& linker){\n";
        for (auto &i : mod.requiredSymbols)
        {
            auto s = std::get<0>(i);
            if (std::holds_alternative<Linker::Symbol::Function>(s.data))
            {
                out << "*reinterpret_cast<void**>(&::" << s.name.module << "::" << s.name.name
                    << ")=std::get<::vwa::Linker::Symbol::Function>(linker.getSymbol({\""
                    << s.name.name << "\",\"" << s.name.module << "\"}).data).ffi_direct;\n";
            }
        }
        out << "}\n"

            << "void InternalExit(){}\n"
            << "#ifndef MODULE_COSTUM_ENTRY_POINT\n"
            << "extern \"C\"{\n"
            << "::vwa::Linker::Module VM_ONLOAD(){return InternalLoad();}\n"
            << "void VM_ONLINK(::vwa::Linker&linker){InternalLink(linker);}\n"
            << "void VM_ONEXIT(){InternalExit();}\n"
            << "}\n"
            << "#endif\n"
            << "#endif\n";

        return {out.str(), mod};
    }
}

int main(int argc, char **argv)
{
    bool interfaceOnly = argc == 3 && !strcmp(argv[1], "-i");
    if (argc != 2 + interfaceOnly)
        throw std::runtime_error("expected exactly one argument");
    std::ifstream input{argv[1 + interfaceOnly]};
    if (!input.is_open())
        throw std::runtime_error("failed to open file");
    vwa::Linker linker;

    std::ofstream output{std::filesystem::path{argv[1]}.replace_extension(".gen.hpp")};
    auto result = vwa::boilerplate::generateBoilerplate(input, linker, interfaceOnly);
    if (!interfaceOnly)
        output << result.first;
    std::ofstream{std::filesystem::path{argv[1]}.replace_extension(".interface")} << linker.serialize(result.second, 1);

    // TODO This should really also output an interface file right away or at least have a flag to do so. Or I'll need
    // to add something to the linker so it can parse this file format
    return 0;
}