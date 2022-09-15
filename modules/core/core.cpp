#define MODULE_IMPL
#include "core.hpp"
using namespace vwa;
namespace core
{
    String make_str(vm_int *str)
    {
        throw std::runtime_error("not implemented");
    }

    void print(String str)
    {
        throw std::runtime_error("not implemented");
    }

    void print_raw(vm_int* str)
    {
        for (; *str;str++)
            putchar(*str);
    }

    void print_c_str(vm_char*c)
    {
        ::printf("%s", c);
    }

    String getLine()
    {
                throw std::runtime_error("not implemented");
    }
    
    vm_int* getLine_raw()
    {
                throw std::runtime_error("not implemented");
    }

    void* malloc(vm_int s)
    {
        return ::malloc(s);
    }

    void free(void*what)
    {
        ::free(what);
    }
}