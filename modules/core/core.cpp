#define MODULE_IMPL
#include "core.gen.hpp"
#include <math.h>
using namespace vwa;
namespace core
{
    String make_str(vm_char* str)
    { 
        auto l = strlen(str);
        auto p = static_cast<char*>(malloc(l));
        memcpy(p, str, l);
        return {l, p};
    }

    void print(String str)
    {
        write(0, str.content, str.length);
    }

    void print_c_str(char*str)
    {
        printf("%s", str);
    }

    vm_char getChar()
    {
        return ::getc(stdin);
    }

    void *malloc(vm_int s)
    {
        return ::malloc(s);
    }

    void free(void *what)
    {
        ::free(what);
    }
    void *realloc(void *current, vm_int size)
    {
        return ::realloc(current, size);
    }
    void *memcpy(void *dest, void *src, vm_int size)
    {
        return ::memcpy(dest, src, size);
    }
    void *memmove(void *dest, void *src, vm_int size)
    {
        return ::memmove(dest, src, size);
    }
    void *calloc(vm_int a, vm_int b)
    {
        return ::calloc(a, b);
    }
    char* itostr(vm_int i){
        auto ret = static_cast<char *>(malloc((int)(i==0?2:(ceil(log10(i)) + 1) * sizeof(char))));
        sprintf(ret, "%d", i);
        return ret;
    }
}