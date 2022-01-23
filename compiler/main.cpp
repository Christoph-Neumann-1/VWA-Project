#include <Preprocessor.hpp>
#include <Lexer.hpp>
#include <CLI/CLI.hpp>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <Parser.hpp>
#include <Compiler.hpp>
#include <Log.hpp>

int main(int argc, char **argv)
{
    using namespace vwa;
    std::string fileName;
    uint loglvl = 3;

    CLI::App app{"VWA Programming language"};
    app.add_option("-f,--file, file", fileName, "Input file")->required()->check(CLI::ExistingFile); // TODO: support for multiple files
    app.add_option("-l,--log-level", loglvl, "Verbosity of the output (0-3)")->check(CLI::Range(0, 3));
    CLI11_PARSE(app, argc, argv);
    FILE *f = fopen(fileName.c_str(), "r");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET); /* same as rewind(f); */

    std::string content;
    content.resize(fsize);
    fread(content.data(), fsize, 1, f);
    fclose(f);

    Logger log;
    log.setStream(Logger::LogLevel::Error, &std::cerr);
    for (uint i = 1; i <= loglvl; i++)
        log.setStream(static_cast<Logger::LogLevel>(i), &std::cout);
    // Preprocessor preprocessor({});
    // content = preprocessor.process(std::move(content));
    auto tokens = tokenize(content, log);
    if (!tokens)
    {
        log << Logger::Error << "Failed to tokenize file" << fileName;
        return 1; // TODO: error codes and logger
    }

    auto tree = buildTree(tokens.value());
    Linker linker;
    auto compiled = compile({{fileName, tree}}, linker, log);
}