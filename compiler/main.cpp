#include <Preprocessor.hpp>
#include <Lexer.hpp>
#include <CLI/CLI.hpp>
#include <cstdio>
#include <filesystem>
#include <iostream>

int main(int argc, char **argv)
{
    std::string fileName;

    CLI::App app{"VWA Programming language"};
    app.add_option("-f,--file, file", fileName, "Input file")->required()->check(CLI::ExistingFile);
    CLI11_PARSE(app, argc, argv);
    FILE *f = fopen(fileName.c_str(), "r");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET); /* same as rewind(f); */

    std::string content;
    content.resize(fsize);
    fread(content.data(), fsize, 1, f);
    fclose(f);
    vwa::Preprocessor preprocessor({});
    content = preprocessor.process(std::move(content));
    auto tokens = vwa::tokenize(content);
    if (!tokens)
    {
        std::cout << "Failed to generate tokens, see log for details.\nExiting.\n";
        return 1; // TODO: error codes and logger
    }
}