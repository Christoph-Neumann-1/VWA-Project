#include <Preprocessor.hpp>
#include <Lexer.hpp>
#include <CLI/CLI.hpp>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <Parser.hpp>
#include <Compiler.hpp>
#include <Log.hpp>
#include <VM.hpp>
#include <chrono>
#include <dlfcn.h>

int main(int argc, char **argv)
{
    using namespace vwa;
    auto compStart = std::chrono::high_resolution_clock::now();
    std::string fileName;
    uint loglvl = 3;
    bool emitTree = false;
    bool PreprocessorOnly = false;

    CLI::App app{"VWA Programming language"};
    app.add_option("-f,--file, file", fileName, "Input file")->required()->check(CLI::ExistingFile); // TODO: support for multiple files
    app.add_option("-l,--log-level", loglvl, "Verbosity of the output (0-3)")->check(CLI::Range(0, 3));
    app.add_flag("-t,--tree", emitTree, "Emit the AST tree");
    app.add_flag("-p,--preprocessor", PreprocessorOnly, "Only preprocess the input file");
    CLI11_PARSE(app, argc, argv);

    std::ifstream input(fileName);

    Logger log;
    log.setStream(Logger::LogLevel::Error, &std::cerr);
    for (uint i = 1; i <= loglvl; i++)
        log.setStream(static_cast<Logger::LogLevel>(i), &std::cout);
    log << Logger::Info << "General Setup completed, beginning compilation\n";
    Preprocessor preprocessor({});
    // TODO: flag to skip preprocessing.
    auto processed = preprocessor.process(input);
    if (PreprocessorOnly || true)
    {
        log << Logger::Info << "Preprocessing completed, exiting\n";
        std::ofstream out("out.vwa");
        out << processed.toString();
        // return 0;
    }
    auto tokens = tokenize(processed, log);
    if (!tokens)
    {
        log << Logger::Error << "Failed to tokenize file " << fileName;
        return 1; // TODO: error codes and logger
    }

    auto tree = buildTree(tokens.value());
    Linker linker;
    auto handle = dlopen("modules/std/bin/std.native", RTLD_LAZY);
    if (!handle)
    {
        log << Logger::Error << "Failed to load std.native";
        log << dlerror();
        return 1;
    }
    auto loadFcn = reinterpret_cast<Linker::Module *(*)()>(dlsym(handle, "MODULE_LOAD"));
    if (!loadFcn)
    {
        log << Logger::Error << "Failed to load module loader";
        log << dlerror();
        return 1;
    }
    auto module = loadFcn();
    module->data = handle;
    linker.provideModule("std", std::move(*module));
    auto compiled = compile({{fileName, tree}}, linker, log);

    const bc::BcToken *main{0};
    for (auto &module : compiled)
    {
        if (module->main)
        {
            if (main)
            {
                log << Logger::Error << "Multiple main functions found\n";
                return 1;
            }
            else
                main = std::get<std::vector<bc::BcToken>>(module->data).data() + module->main - 1;
        }
    }
    if (!main)
    {
        log << Logger::Error << "No entry point found\n";
        return -1;
    }
    auto compEnd = std::chrono::high_resolution_clock::now();
    log << Logger::Info << "Finished compiling in " << std::chrono::duration_cast<std::chrono::milliseconds>(compEnd - compStart).count() << "ms\n";

    VM vm;
    // bc::BcToken code[] = {
    //     bc::Push64,
    //     2,0,0,0,
    //     0,0,0,0,
    //     bc::JumpFuncRel,
    //     18,0,0,0,
    //     0,0,0,0,
    //     8,0,0,0,
    //     0,0,0,0,
    //     bc::Exit,
    //     bc::Push,
    //     16, 0, 0, 0,
    //     0, 0, 0, 0,
    //     bc::ReadRel,
    //     16, 0, 0, 0,
    //     0, 0, 0, 0,
    //     8,0,0,0,
    //     0,0,0,0,
    //     bc::Dup,
    //     8, 0, 0, 0,
    //     0, 0, 0, 0,
    //     bc::WriteRel,
    //     24,0,0,0,
    //     0,0,0,0,
    //     8,0,0,0,
    //     0,0,0,0,
    //     bc::WriteRel,
    //     32,0,0,0,
    //     0,0,0,0,
    //     8,0,0,0,
    //     0,0,0,0,
    //     bc::ReadRel,
    //     24,0,0,0,
    //     0,0,0,0,
    //     8,0,0,0,
    //     0,0,0,0,
    //     bc::ReadRel,
    //     32,0,0,0,
    //     0,0,0,0,
    //     8,0,0,0,
    //     0,0,0,0,
    //     bc::AddI,
    //     bc::Return,
    //     8,0,0,0,
    //     0,0,0,0,};
    log << Logger::Info << "Executing main function\n";
    log << Logger::Info << ColorI("BEGIN PROGRAM OUTPUT\n\n");
    auto begin = std::chrono::high_resolution_clock::now();
    auto res = vm.exec(main);
    auto end = std::chrono::high_resolution_clock::now();

    log << "\n\n"
        << Logger::Info << ColorI("END PROGRAM OUTPUT\n");
    log << (res.statusCode > VM::ExitCode::Exit ? Logger::Error : Logger::Info);
    switch (res.statusCode)
    {
    case VM::ExitCode::Success:
        log << "Main function returned " << *reinterpret_cast<int64_t *>(vm.stack.top - 8);
        break;
    case VM::ExitCode::Exit:
        log << "Exit was called with return code " << res.exitCode;
        break;
    default:
        log << "Program terminated abnormally. Error " << res.statusCode << " Exit Code " << res.exitCode;
    }
    log << '\n';

    log << Logger::Info << "Finished execution in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "ms\n";
    log << Logger::Info << "Total time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - compStart).count() << "ms\n";
    return res.statusCode == VM::ExitCode::Success ? *reinterpret_cast<int64_t *>(vm.stack.top - 8) : -1;
}