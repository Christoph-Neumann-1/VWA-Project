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
#include <exception>

int main(int argc, char **argv)
{
    using namespace vwa;
    auto compStart = std::chrono::high_resolution_clock::now();
    std::vector<std::string> fileNames;
    uint loglvl = 3;
    bool emitTree = false;
    bool PreprocessorOnly = false;
    bool execute = false;
    bool usestdin = false;
    bool interfaceOnly{false};
    Logger log;
    log.setStream(Logger::LogLevel::Error, &std::cerr);
    for (uint i = 1; i <= loglvl; i++)
        log.setStream(static_cast<Logger::LogLevel>(i), &std::cout);
    // TODO: automatically detect compiled input

    // OK,so in order to not process anything after --, I search for it and just not tell cli11 about them
    auto posAfter = std::find_if(argv, argv + argc, [](auto s)
                                 { return !strcmp(s, "--"); });
    auto argc2 = std::distance(argv, posAfter);
    CLI::App app{"VWA Programming language"};
    // TODO: if no file is specified, use stdin
    app.add_option("-f,--file, files", fileNames, "Input files")->check(CLI::ExistingFile)->required();
    app.add_option("-l,--log-level", loglvl, "Verbosity of the output (0-3)")->check(CLI::Range(0, 3));
    app.add_flag("-t,--tree", emitTree, "Emit the AST tree");
    app.add_flag("-p,--preprocessor", PreprocessorOnly, "Only preprocess the input file");
    app.add_flag("-r,--run", execute, "Run the file provided")->excludes("-t")->excludes("-p");
    app.add_flag("-i,--interfaceOnly", interfaceOnly, "Only generate interface files")->excludes("-p", "-r");
    CLI11_PARSE(app, argc2, argv);

    if (execute)
    {
        std::ifstream t(fileNames.at(0));
        t.seekg(0, std::ios::end);
        size_t size = t.tellg();
        t.seekg(0);
        std::unique_ptr<char[]> buf(new char[size]);
        t.read(buf.get(), size);
        Linker l;
        l.provideModule(l.deserialize({buf.get(), size}));
        l.satisfyDependencies();
        l.patchAddresses();
        auto main = l.findMain();
        if (!~reinterpret_cast<uint64_t>(main))
        {
            log << Logger::Error << "Multiple entry points\n";
            return -1;
        }
        if (!main)
        {
            log << Logger::Error << "No entry point\n";
            return -1;
        }

        VM vm;
        log << Logger::Info << "Executing main function\n";
        vm.setupStack();
        vm.stack.push<int64_t>(argc - argc2);
        vm.stack.push<uint64_t>(reinterpret_cast<uint64_t>(vm.stack.top)+8);
        for (int i = argc2+1; i < argc; i++)
            vm.stack.push(argv[i]);
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
    else
    {
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
        if (PreprocessorOnly)
        {
            for (size_t i = 0; i < fileNames.size(); i++)
                std::ofstream(fileNames[i].append(".pp")) << processed[i].toString();
            return 0;
        }
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
        // auto handle = dlopen("modules/std/bin/std.native", RTLD_LAZY);
        // if (!handle)
        // {
        //     log << Logger::Error << "Failed to load std.native";
        //     log << dlerror();
        //     return 1;
        // }
        // auto loadFcn = reinterpret_cast<Linker::Module (*)()>(dlsym(handle, "MODULE_LOAD"));
        // if (!loadFcn)
        // {
        //     log << Logger::Error << "Failed to load module loader";
        //     log << dlerror();
        //     return 1;
        // }
        // auto module = loadFcn();
        // module.data.emplace<Linker::Module::DlHandle>(handle);
        // linker.provideModule(std::move(module));
        for (size_t i = 0; i < fileNames.size(); ++i)
            trees[i].name = fileNames[i]; // TODO: make sure all identifiers used internally are updated
        // For some reason initializer lists don't work with move only types
        // I hope I am allowed to move out of the temporary
        if (interfaceOnly)//TODO: actually implement
        {
            for (auto &m : trees)
                std::ofstream(m.name + ".interface")<<linker.serialize(m,1);
            return 0;
        }

        auto compiled = compile(std::move(trees), linker, log);
        // std::ofstream("") linker.serialize(*compiled[0]);
        // Linker l2;
        // dlopen("modules/std/bin/std.native", RTLD_LAZY);
        // auto m = loadFcn();
        // m.data.emplace<Linker::Module::DlHandle>(handle);
        // l2.provideModule(std::move(m));
        for (auto c : compiled)
        {
            // auto serialized = linker.serialize(*c, false);
            // log << Logger::Debug << "Testing Serialization: " << c->name << '\n'
            //     << (m == *c ? "Success" : "Failure") << '\n'; // TODO: debug equality of requiredSymbols
            // // log << Logger::Debug << "Trying to find issue with requiredsymbols\n";
            // // for (size_t i = 0;i<m.requiredSymbols.size();i++)
            // //     log<<"Symbol: "<<i<< (m.requiredSymbols[i]==c->requiredSymbols[i]?" matches":" doesn't match")<<'\n';
            std::ofstream(c->name + ".bc") << linker.serialize(*c, 0);
        }
        // l2.satisfyDependencies();
        // l2.patchAddresses();
        // linker.satisfyDependencies();
        // linker.patchAddresses();

        // // auto compiled = compile(({ std::vector<Linker::Module> tmp; tmp.emplace_back(std::move(tree));std::move(tmp); }), linker, log);

        // // TODO: call some kind of function to load definitions for functions for which only declarations exist

        // // linker.patchAddresses();

        // auto main = linker.findMain();
        // // const bc::BcToken *main{0};
        // // for (auto &module : compiled)
        // // {
        // //     auto sym = linker.tryFind({"main", module->name});
        // //     if (sym)
        // //     {
        // //         if (main)
        // //         {
        // //             log << Logger::Error << "Multiple main functions found";
        // //             return 1;
        // //         }
        // //         main = std::get<0>(sym->data).idx + std::get<3>(module->data).data();
        // //     }
        // // }
        // if (!~reinterpret_cast<uint64_t>(main))
        // {
        //     log << Logger::Error << "Multiple entry points\n";
        //     return -1;
        // }
        // if (!main)
        // {
        //     log << Logger::Error << "No entry point\n";
        //     return -1;
        // }
        // auto compEnd = std::chrono::high_resolution_clock::now();
        // log << Logger::Info << "Finished compiling in " << std::chrono::duration_cast<std::chrono::milliseconds>(compEnd - compStart).count() << "ms\n";

        // VM vm;
        // // bc::BcToken code[] = {
        // //     bc::Push64,
        // //     2,0,0,0,
        // //     0,0,0,0,
        // //     bc::JumpFuncRel,
        // //     18,0,0,0,
        // //     0,0,0,0,
        // //     8,0,0,0,
        // //     0,0,0,0,
        // //     bc::Exit,
        // //     bc::Push,
        // //     16, 0, 0, 0,
        // //     0, 0, 0, 0,
        // //     bc::ReadRel,
        // //     16, 0, 0, 0,
        // //     0, 0, 0, 0,
        // //     8,0,0,0,
        // //     0,0,0,0,
        // //     bc::Dup,
        // //     8, 0, 0, 0,
        // //     0, 0, 0, 0,
        // //     bc::WriteRel,
        // //     24,0,0,0,
        // //     0,0,0,0,
        // //     8,0,0,0,
        // //     0,0,0,0,
        // //     bc::WriteRel,
        // //     32,0,0,0,
        // //     0,0,0,0,
        // //     8,0,0,0,
        // //     0,0,0,0,
        // //     bc::ReadRel,
        // //     24,0,0,0,
        // //     0,0,0,0,
        // //     8,0,0,0,
        // //     0,0,0,0,
        // //     bc::ReadRel,
        // //     32,0,0,0,
        // //     0,0,0,0,
        // //     8,0,0,0,
        // //     0,0,0,0,
        // //     bc::AddI,
        // //     bc::Return,
        // //     8,0,0,0,
        // //     0,0,0,0,};
        // log << Logger::Info << "Executing main function\n";
        // vm.setupStack();
        // // TODO: make this use the real arguments
        // int64_t firstArg[] = {'t', 'e', 's', 't', '\0'};
        // int64_t *args[1] = {firstArg};

        // vm.stack.push<int64_t>(1);
        // vm.stack.push<void *>(args);
        // log << Logger::Info << ColorI("BEGIN PROGRAM OUTPUT\n\n");
        // auto begin = std::chrono::high_resolution_clock::now();
        // auto res = vm.exec(main);
        // auto end = std::chrono::high_resolution_clock::now();

        // log << "\n\n"
        //     << Logger::Info << ColorI("END PROGRAM OUTPUT\n");
        // log << (res.statusCode > VM::ExitCode::Exit ? Logger::Error : Logger::Info);
        // switch (res.statusCode)
        // {
        // case VM::ExitCode::Success:
        //     log << "Main function returned " << *reinterpret_cast<int64_t *>(vm.stack.top - 8);
        //     break;
        // case VM::ExitCode::Exit:
        //     log << "Exit was called with return code " << res.exitCode;
        //     break;
        // default:
        //     log << "Program terminated abnormally. Error " << res.statusCode << " Exit Code " << res.exitCode;
        // }
        // log << '\n';

        // log << Logger::Info << "Finished execution in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "ms\n";
        // log << Logger::Info << "Total time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - compStart).count() << "ms\n";
        // return res.statusCode == VM::ExitCode::Success ? *reinterpret_cast<int64_t *>(vm.stack.top - 8) : -1;
    }
}