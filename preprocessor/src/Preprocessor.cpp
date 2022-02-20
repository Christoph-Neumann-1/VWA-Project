#include <Preprocessor.hpp>

namespace vwa
{

    Preprocessor::File Preprocessor::process(std::istream &input)
    {
        File f{input};
        return f;
    }
}