#pragma once
#include <vector>
#include <string>
#include <variant>
#include <filesystem>
#include <unordered_map>
// I will figure this out later
namespace vwa
{
    class Preprocessor
    {
    public:
        struct Options
        {
            std::vector<std::filesystem::path> searchPath;
            std::vector<std::pair<std::string, std::variant<int32_t, std::string>>> defines;
            uint64_t maxExpansionDepth = 100;
        };
        Preprocessor(Options options_) : options(options_) {}

        std::string process(std::string input);

    private:
        const Options options;
        uint current;
        std::string file;
        std::unordered_map<std::string, std::variant<int32_t, std::string>> vars;
    };
}
