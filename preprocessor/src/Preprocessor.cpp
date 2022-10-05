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
        while (current.it!=file.end())
        {
            // Suggestion: \ for line endings. No exception. $ for when you need to escape a macro for some reason. if you want to use the literal $(only possible in string, prefix it another $)
            if (*current == '$')
            {
                if (current.pos < current.it->str.length() - 1)
                {
                    ++current;
                    if (*current == '#' || *current == '$')
                    {
                        current.it->str.erase(current.pos-1, 1);
                        // Best way of bounds checking for now
                        current++;
                        current--;
                    }
                    continue;
                }
            }
            else if (*current == '#')
            {
                if (current.pos >= current.it->str.length() - 1)
                    throw std::runtime_error("Unexpected end of line");
                ++current;
                if (*current == ' ')
                    current.it->str.erase(current.pos - 1);
                current.pos = 0;
                continue;
            }

            ++current;
        }
        return file;
    }
}