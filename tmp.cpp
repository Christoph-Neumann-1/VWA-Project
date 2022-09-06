/*
This is the basic structure of my linker
Requirements:
- Provide a efficient, easy to use data structure for holding symbol information, which can ideally be reused by the compiler
- Find definitions.
- - Definitions can be sourced from a .interface file, directly provided by the compiler, or if no interface file is present, then either a native module or a .vwa file is used to generate a interface file.
- - These files are NOT compiled by the linker, they processed just far enough to generate the interface file.
- Replace helper instructions with the correct instruction(ffi or norman) and patch adressed
- Replace internal references with relative adresses
- The linker needs to parse .module files and load the code contained within
- It needs to provide meaningful output to the user.
- It needs to find conflicts in symbols, but also detect things like this:
    Module A:
        struct Bar{a:int}
    Module B:
        struct Bar{b:int, c:int}
    Module C:
        import "A";
        func foo(bar:Bar){}
    Module D:
        import "B";
        import "C";
        func main()
        {
            var b:Bar;
            foo(b);
        }
    Currently this will compile just fine because imports are not exported by default, this needs to change. At the very least all symbols used in functions need to be exported.
    Ideally I should replace the current behaviour not exporting by default with always exporting by default and only keeping things private when explicitly told to.
    An alternative would be to check for all imported symbols if the defintion  used by the module matches the definition in the current module.

- Solve problems with dependency cycles.
- Provide some sane way to separate modules from one another, if I just dump all symbols of all imported modules into one big table, there WILL be collisions.

*/

#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <unordered_set>

using ffi_func = void (*)(void *vm); // This obviously needs to be changed to a proper type

struct Identifier
{
    std::string name;
    std::string module_;
};

namespace std
{
    template <>
    struct hash<Identifier>
    {
        // I really hope this works
        size_t operator()(const Identifier &id) const
        {
            return hash<string>()(id.name) ^ hash<string>()(id.module_);
        }
    };
}
struct Sym;

struct Type
{
    Identifier id;
    uint pointerDepth; // If this ever overflows, then that is a feature, preventing that horrible code of yours from ever torturing that poor processor.
};
// TODO: refcounting
struct Module
{
    std::string name;                             // Useful because all types need an explicit module name, this allows adding it where omitted.
    uint major, minor, patch;                     // Module versions, where applicable. I may use this to check for compatibility at runtime.
    bool containsDefinitions;                     // Whether code is available
    std::variant<void *, std::vector<char>> code; // Either bytecode or a native module handle.
    std::vector<std::string> imports;
    // TODO: How am I going to differentiate between internal and external definitions, ideally with no runtime overhead?
    struct Symbol
    {
        struct Param
        {
            Type type;
            bool isConst;
        };
        struct Function
        {
            // I could use bitfiels for packing things, but it's not worth it.
            std::vector<Param> params;
            Param returnType;
            union
            {
                ffi_func ffi;
                char *byteCode;
                size_t index; // Used during compilation.
            };
            bool isConstexpr;
            enum Type : char
            {
                Undefined,
                FFI,
                Normal,
            };
            Type type = Undefined;
        };
        struct Type
        {
            std::vector<Param> params;
        };
        std::variant<Function, Type> data;
    };

    // No local symbols are stored here, non exported symbols are handled by the compiler
    std::vector<std::pair<Identifier, Symbol>> exports;                                     // Duplicates are checked for at other points anyways, so this is ok
    std::vector<std::pair<Identifier, std::variant<Symbol, Symbol *>>> requiredDefinitions; // During compilation this gets filled with all symbols which are actually in use once compilation is done.
    // It also serves as a lookup table for the linker.
    // TODO: where am I going to count references and how?
};
class Linker
{
    std::unordered_map<Identifier, Sym *> symbols;
    std::unordered_set<std::string> modules; // Used to ensure no duplicates are present.
    Sym *lookupSymbol(const Identifier &id)
    {
        if (auto it = symbols.find(id); it != symbols.end())
            return it->second;
        return nullptr;
    }
};
