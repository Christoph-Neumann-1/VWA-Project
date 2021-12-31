#include <Preprocessor.hpp>

namespace vwa
{
    std::string Preprocessor::process(std::string input)
    {
        file = std::move(input);
        current = 0;
        vars.clear();
        for (auto &var : options.defines)
        {
            vars.emplace(var.first, var.second);
        }
        std::replace(file.begin(), file.end(), '\n', ' ');
        return std::move(file);
    }
}