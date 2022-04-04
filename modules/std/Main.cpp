#include <Linker.hpp>
#include <Log.hpp>
#include <VM.hpp>
#include <fmt/core.h>

// TODO: automatic casting(meta function to convert to supported types)

namespace vwa
{
    namespace
    {
        struct Autorun
        {
            template <typename T>
            Autorun(T func) { func(); }
        };
    }

    template <typename... Args>
    struct ParameterReader
    {
        template <size_t prev, typename T = void, typename... Args2>
        struct Offset;
        template <size_t prev>
        struct Offset<prev, void>
        {
            template <size_t idx>
            static void get(uint8_t *) { throw std::runtime_error("This should not be possible"); }
        };
        template <size_t prev, typename T, typename... Args2>
        struct Offset
        {
            static constexpr size_t offset = prev;
            using next = Offset<prev + sizeof(T), Args2...>;
            // I still don't know where this terminates
            using type = T;
            // TODO: use requires to constrain this
            template <size_t idx, typename current>
            struct Get
            {
                static constexpr auto value = Get<idx - 1, typename current::next>::value;
                using type = typename next::template Get<idx - 1, typename current::next>::type;
            };
            template <typename current>
            struct Get<0, current>
            {
                static constexpr auto value = current::offset;
                using type = current::type;
            };

            template <size_t idx>
            static auto get(uint8_t *start)
            {
                using T2 = Get<idx, Offset<prev, T, Args2...>>;
                return *reinterpret_cast<typename T2::type *>(start + T2::value);
            }
        };
        using offsets = Offset<0, Args...>;
        using sequence = std::make_integer_sequence<size_t, sizeof...(Args)>;
    };
    template <typename R, typename... Args>
    void WrapFunc(VM *vm, R (*func)(Args...))
    {
        using reader = ParameterReader<Args...>;
        auto expand = [func]<size_t... idx>(uint8_t * start, std::integer_sequence<size_t, idx...>)
        {
            if constexpr (std::is_void_v<R>)
                (func(reader::offsets::template get<idx>(start)), ...);
            else
                return func(reader::offsets::template get<idx>(start)...);
        };
        vm->stack.top -= (sizeof(Args) + ... + 0);
        if constexpr (std::is_void_v<R>)
            expand(vm->stack.top, typename reader::sequence{});
        else
            vm->stack.push(expand(vm->stack.top, typename reader::sequence{}));
    }

    // TODO: pointers(maybe return string and keeps appending to it)
    template <typename T>
    constexpr std::string_view getTypeName()
    {
        if constexpr (std::is_pointer_v<T>)
        {
            // TODO
        }
        return T::typeName;
    }

    template <>
    constexpr std::string_view getTypeName<void>()
    {
        return "void";
    }
    template <>
    constexpr std::string_view getTypeName<char>()
    {
        return "char";
    }
    template <>
    constexpr std::string_view getTypeName<int64_t>()
    {
        return "int";
    }
    template <>
    constexpr std::string_view getTypeName<double>()
    {
        return "double";
    }
    template <typename T, size_t S = 0>
    struct getPointerLvl
    {
        static constexpr size_t value = S;
        static constexpr std::string_view typeName = getTypeName<T>();
    };
    template <typename T, size_t S>
    struct getPointerLvl<T *, S> : getPointerLvl<T, S + 1>
    {
    };
    // TODO: auto cast
    template <typename Ret, typename... Args>
    std::pair<std::vector<Linker::Module::Symbol::Function::Parameter>, Linker::Module::Symbol::Function::Parameter> generateSignature(Ret (*f)(Args...))
    {
        std::vector<Linker::Module::Symbol::Function::Parameter> params{{Identifier{std::string{getPointerLvl<Args>::typeName}}, getPointerLvl<Args>::value}...};
        return {params, {Identifier{std::string{getPointerLvl<Ret>::typeName}}, getPointerLvl<Ret>::value}};
    }
}

// Exporting Structs sucks because c++ lacks reflection, so I recommend including an additional file with the sole purpose of declaring structs in the language.
// These structs can then be included from here

#define IMPORT_FUNC(type, name)                  \
    namespace                                    \
    {                                            \
        using ffiImportT = type;                 \
        ffiImportT name{0};                      \
        /*TODO: add hook to initialize pointer*/ \
    }

#define VWA_INTERNAL_UNIQUE_NAME(name) CONCAT(vwa_internal_##name, __COUNTER__)

#define CONCAT(a, b) CONCAT_IMPL(a, b)
#define CONCAT_IMPL(a, b) a##b

using namespace vwa;
Linker::Module mod;
#define ExportFunctionAs(f, name)                                                                                                                                                                                        \
    namespace                                                                                                                                                                                                            \
    {                                                                                                                                                                                                                    \
        vwa::Autorun VWA_INTERNAL_UNIQUE_NAME(autorun){                                                                                                                                                                  \
            []() {                                                                                                                                                                                                       \
                auto def = generateSignature(f);                                                                                                                                                                         \
                mod.exportedSymbols.push_back({{#name}, Linker::Module::Symbol::Function{Linker::Module::Symbol::Function::Type::External, {.ffi = WRAP_FUNC(f)}, std::move(def.first), std::move(def.second), false}}); \
            }};                                                                                                                                                                                                          \
    }

#define ExportFunction(f) ExportFunctionAs(f, f)

#define WRAP_FUNC(f) \
    [](vwa::VM *vm) { vwa::WrapFunc(vm, f); }

// TODO: args from func ptr

// TODO: special type of function supporting calling back into vm.

// void add(VM *vm)
// {
//     vm->stack.push(vm->stack.pop<int64_t>() + vm->stack.pop<int64_t>());
// }

void sayHello(VM *vm)
{
    fmt::print("Hello World!\n");
}

int64_t readChar()
{
    return std::getchar();
}

// Returns an array of bytes each containing a single bit. This is a stop gap solution till I implement bitwise operators
char *getBits(void *ptr, int64_t size)
{
    char *bits = reinterpret_cast<char *>(malloc(size * 8));
    for (int64_t i = 0; i < size; i++)
    {
        for (int64_t j = 0; j < 8; j++)
        {
            bits[i * 8 + j] = ((reinterpret_cast<uint8_t *>(ptr)[i] >> j) & 1);
        }
    }
    return bits;
}

ExportFunction(getBits);
void *m_malloc(int64_t size)
{
    return malloc(size);
}
ExportFunctionAs(m_malloc, malloc);
ExportFunction(free);

void printChar(int64_t c)
{
    std::putchar(c);
}
void print(int64_t *str)
{
    while (*str!='\0')
        putchar(*str++);
}
ExportFunction(print);
ExportFunction(readChar);
ExportFunction(printChar);

int64_t add(int64_t a, int64_t b) { return a + b; }

ExportFunction(add);

int64_t *intArray(int64_t size)
{
    return new int64_t[size];
}

void delIntArray(int64_t *arr)
{
    delete[] arr;
}

// Don't call this more than once
// Returns all symbols and imports
extern "C" Linker::Module *MODULE_LOAD()
{
    // printf(ColorD("Debug:") ResetColor "Loading std library\n");
    // mod.exportedSymbols.push_back({"add", Linker::Module::Symbol::Function{Linker::Module::Symbol::Function::Type::External, {.ffi = add}, {{"int", 0}, {"int", 0}}, {"int", 0}, false}});
    mod.exportedSymbols.push_back({Identifier{"sayHello", "std"}, Linker::Module::Symbol::Function{Linker::Module::Symbol::Function::Type::External, {.ffi = sayHello}, {}, {{"void"}, 0}, false}});
    mod.exportedSymbols.push_back({Identifier{"intArray"}, Linker::Module::Symbol::Function{Linker::Module::Symbol::Function::Type::External, {.ffi = WRAP_FUNC(intArray)}, {{{"int"}, 0}}, {{"int"}, 1}, false}});
    mod.exportedSymbols.push_back({Identifier{"delIntArray"}, Linker::Module::Symbol::Function{Linker::Module::Symbol::Function::Type::External, {.ffi = WRAP_FUNC(delIntArray)}, {{{"int"}, 1}}, {{"void"}, 0}, false}});
    return &mod;
}

// Gets called after linking, in here function pointers are resolved
extern "C" void MODULE_SETUP()
{
}

// In case there are any cleanup functions
extern "C" void MODULE_UNLOAD()
{
}