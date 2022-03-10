#include <Preprocessor.hpp>

namespace vwa
{

    enum class EscapeType
    {
        File,        // Top level
        MacroInvoke, //#A() for example
    };

    void handleEscape(Preprocessor::File::charIterator &it, EscapeType type, Preprocessor::File &file)
    {
    }

    Preprocessor::File Preprocessor::process(std::istream &input)
    {
        File file = File{input};
        File::charIterator current = {file.begin()};
        std::unordered_map<std::string, File> vars;
        while (current.it.get()->next.get())
        {
            switch (*current)
            {
            case '\\':
                if (current.pos == current.it->str.size() - 1)
                {
                    current.it->str.pop_back();
                    current.it->str += (current.it + 1)->str;
                    file.erase(current.it + 1);
                    continue;
                }
            case '#':
            {
                char prev = current.pos ? *(current - 1) : 0;
                switch (prev)
                {
                case '\\':
                {
                    current.it->str.erase(current.it->str.begin() + current.pos - 1);
                    current++ --; //To advance to the next line if the character was at the end
                    continue;
                }
                default:
                    // TODO: should I allow multi line comments?
                    if (current.pos)
                    {
                        current.it->str.erase(current.it->str.begin() + current.pos, current.it->str.end());
                        ++current.it;
                        current.pos = 0;
                        continue;
                    }
                    current.it = file.erase(current.it);
                    current.pos = 0;
                    continue;
                }
            }
            }
            current++;
        }

        return file;
    }
}