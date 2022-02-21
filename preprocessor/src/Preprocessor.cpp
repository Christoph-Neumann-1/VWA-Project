#include <Preprocessor.hpp>

namespace vwa
{

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
            }
            ++current;
        }

        return file;
    }
}