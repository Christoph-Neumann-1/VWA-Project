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
    std::vector<std::string> fileNames;
    uint loglvl = 3;
    bool emitTree = false;
    bool PreprocessorOnly = false;

    CLI::App app{"VWA Programming language"};
    app.add_option("-f,--file, files", fileNames, "Input files")->required()->check(CLI::ExistingFile); // TODO: support for multiple files
    app.add_option("-l,--log-level", loglvl, "Verbosity of the output (0-3)")->check(CLI::Range(0, 3));
    app.add_flag("-t,--tree", emitTree, "Emit the AST tree");
    app.add_flag("-p,--preprocessor", PreprocessorOnly, "Only preprocess the input file");
    CLI11_PARSE(app, argc, argv);

    Logger log;
    log.setStream(Logger::LogLevel::Error, &std::cerr);
    for (uint i = 1; i <= loglvl; i++)
        log.setStream(static_cast<Logger::LogLevel>(i), &std::cout);
    log << Logger::Info << "General Setup completed, beginning compilation\n";

    Preprocessor preprocessor({});

    std::vector<Preprocessor::File> processed;
    processed.reserve(fileNames.size());

    // TODO: flag to skip preprocessing.
    for (auto &file : fileNames)
    {
        std::ifstream stream{file};
        processed.push_back(preprocessor.process(stream));
    }
    // if (PreprocessorOnly || true)
    // {
    //     log << Logger::Info << "Preprocessing completed, exiting\n";
    //     std::ofstream out("out.vwa");
    //     out << processed.toString();
    //     // return 0;
    // }
    std::vector<std::vector<Token>> tokens;
    tokens.reserve(fileNames.size());
    for (size_t i = 0; i < processed.size(); i++)
        if (auto r = tokenize(processed[i], log); !r)
        {
            log << Logger::Error << "Failed to tokenize file " << fileNames[i];
            return 1; // TODO: error codes and logger
        }
        else
        {
            tokens.push_back(std::move(*r));
        }
    std::vector<Linker::Module> trees;
    for (auto &t : tokens)
        trees.push_back(buildTree(t));
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
    module->data.emplace<Linker::Module::DlHandle>(handle);
    linker.provideModule(std::move(*module));
    for (size_t i = 0; i < fileNames.size(); ++i)
        trees[i].name = fileNames[i]; // TODO: make sure all identifiers used internally are updated
    // For some reason initializer lists don't work with move only types
    // I hope I am allowed to move out of the temporary
    auto compiled = compile(std::move(trees), linker, log);
    linker.satisfyDependencies();
    linker.patchAddresses();

    // auto compiled = compile(({ std::vector<Linker::Module> tmp; tmp.emplace_back(std::move(tree));std::move(tmp); }), linker, log);

    // TODO: call some kind of function to load definitions for functions for which only declarations exist

    // linker.patchAddresses();

    const bc::BcToken *main{0};
    for (auto &module : compiled)
    {
        auto sym = linker.tryFind({"main", module->name});
        if (sym)
        {
            if (main)
            {
                log << Logger::Error << "Multiple main functions found";
                return 1;
            }
            main = std::get<0>(sym->data).idx + std::get<3>(module->data).data();
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
    vm.setupStack();
    // TODO: make this use the real arguments
    int64_t firstArg[] = {'t', 'e', 's', 't', '\0'};
    int64_t *args[1] = {firstArg};

    vm.stack.push<int64_t>(1);
    vm.stack.push<void *>(args);
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