#include <Preprocessor.hpp>
#include <variant>

namespace vwa
{

    // enum class EscapeType
    // {
    //     File,        // Top level
    //     MacroInvoke, //#A() for example
    // };

    void handleEscape(Preprocessor::File::charIterator &current, Preprocessor::File &file)
    {
        switch (*(++current))
        {
        }
    }

    Preprocessor::File Preprocessor::process(std::istream &input)
    {
        File file = File{input};
        File::charIterator current = file.begin();
        while (current.it.get()->next.get())
        {
            current.pos = current.it->str.length() - 1;
            if (*current == '\\')
                if (current.pos == current.it->str.size() - 1)
                {
                    current.it->str.pop_back();
                    current.it->str += (current.it + 1)->str;
                    file.erase(current.it + 1);
                    current.pos = current.it->str.length() - 1;
                    continue;
                }
            ++current.it;
        }

        std::unordered_map<std::string, std::variant<int64_t, File>> vars;
        vars.reserve(options.defines.size());
        for (auto &d : options.defines)
            vars.emplace(d.first, std::variant<int64_t, File>{d.second});
        current = file.begin();
        while (current.it != file.end())
        {
            // Suggestion: \ for line endings. No exception. $ for when you need to escape a macro for some reason. if you want to use the literal $(only possible in string, prefix it another $)
            if (*current == '#')
            {
                if (current.pos < current.it->str.length() - 1)
                {
                    // Let's make this all be a language commands
                    ++current;
                    switch (*current)
                    {
                    case '#':
                        // Escape rule
                        current.it->str.erase(current.pos - 1, 1);
                        // Best way of bounds checking for now
                        current++;
                        current--;
                        break;
                    case ' ':
                        // Comments
                        if (current.pos == 1)
                            current = file.erase(current.it);
                        else
                            current.it->str.erase(current.pos - 1), current = current.it + 1;
                        break;
                    default:
                        // Assume this to be some kind of instruction e.g. define macro, do computation, substitute. Lookup as follows:
                        // a dictionary of commands which cover everything from comments to escaping. Some simple permission flags to differentiate user defined
                        // macros from built-in ones and reserved keys(#n). Also: all macro invocations need to be enclosed withing # with arguments being enclosed
                        // in , for which I still need some escape rules. Requirements for command classes: modify internal state when necessary, cover everything
                        // with as few virtual calls as possible. Extendable by native libraries? Able to be saved in compiled libraries?(add a library switch just for that case)
                        // Otherwise add a #include to dedup macros
                        std::string name{*current};
                        while ((++current).it != file.end())
                        {
                            switch (*current)
                            {
                            case '#':
                            case ' ':
                                goto processArgs;
                            default:
                                name += *current;
                            }
                        }
                    processArgs:
                        std::vector<std::string> arguments;
                        while (current.it != file.end())
                        {
                            switch (*current)
                            {
                            case '#':
                                current++;
                                goto invoke;
                            case ' ':
                                throw std::runtime_error("NOT IMPLEMENTED. WELL, THAT SUCKS GUESS YOU ARE GOING TO HAVE TO MAKE DO WITHOUT IT.");
                            }
                        }

                        loop12:
                    arguments . push_back({*current});
                    if((++ current ) . it!=file . end())
                            std::printf("You should really rethink your life decisions if you are using this\n"), ({ goto loop12; });
                        throw std::runtime_error("Processing error whilst processing preprocessor invocation");
                    invoke:
                        // Whitespace followed by length(optional) another whitespace and command.
                        // If no length is provided, there must not be any '#' or ' '
                        // Arg processing must end with a #. Anything else is treated as invalid
                        break;
                    }

                    continue;
                }
                else
                {
                    // TODO: join lines
                }
            }
            // else if (*current == '#')
            // {
            //     if (current.pos >= current.it->str.length() - 1)
            //         throw std::runtime_error("Unexpected end of line");
            //     ++current;
            //     if (*current == ' ')
            //         current.it->str.erase(current.pos - 1);
            //     current.pos = 0;
            //     continue;
            // }

            ++current;
        }
        return file;
    }
}