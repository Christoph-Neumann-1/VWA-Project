#include <VM.hpp>
#include <Linker.hpp>
namespace vwa
{
using vm_int=int64_t;
using vm_float = double;
using vm_char = char;
}
#define VM_STRUCT struct __attribute__((packed))
namespace core{
VM_STRUCT String;
}
namespace core{
VM_STRUCT String
{
::vwa::vm_int length;
::vwa::vm_int* content;
};
}
namespace core
{
::core::String make_str(::vwa::vm_int*);
void print(::core::String);
void print_raw(::vwa::vm_int*);
void print_c_str(::vwa::vm_char*);
::core::String getLine();
::vwa::vm_int* getLine_raw();
void* malloc(::vwa::vm_int);
void free(void*);
void* realloc(void*,::vwa::vm_int);
void* memcpy(void*,void*,::vwa::vm_int);
void* memmove(void*,void*,::vwa::vm_int);
void* calloc(::vwa::vm_int,::vwa::vm_int);
}
#ifdef MODULE_IMPL
::vwa::Linker::Module InternalLoad(){
::vwa::Linker::Module module{
.name="core",
.requiredSymbols={
::vwa::Linker::Symbol{{"String","core"},::vwa::Linker::Symbol::Struct{{{"length",{{"int",""},0}},{"content",{{"int",""},1}},}}},
},
.exports={
::vwa::Linker::Symbol{{"String","core"},::vwa::Linker::Symbol::Struct{{{"length",{{"int",""},0}},{"content",{{"int",""},1}},}}},
::vwa::Linker::Symbol{{"make_str","core"},::vwa::Linker::Symbol::Function{.type=::vwa::Linker::Symbol::Function::External,.params={{.type={{"int",""},1}},},.returnType={{"String","core"},0},.ffi=[](::vwa::VM *vm){::vwa::vm_int* p0;p0=vm->stack.pop<::vwa::vm_int*>();vm->stack.push(::core::make_str(p0));},.ffi_direct=reinterpret_cast<void*>(::core::make_str)}},
::vwa::Linker::Symbol{{"print","core"},::vwa::Linker::Symbol::Function{.type=::vwa::Linker::Symbol::Function::External,.params={{.type={{"String","core"},0}},},.returnType={{"void",""},0},.ffi=[](::vwa::VM *vm){::core::String p0;p0=vm->stack.pop<::core::String>();::core::print(p0);},.ffi_direct=reinterpret_cast<void*>(::core::print)}},
::vwa::Linker::Symbol{{"print_raw","core"},::vwa::Linker::Symbol::Function{.type=::vwa::Linker::Symbol::Function::External,.params={{.type={{"int",""},1}},},.returnType={{"void",""},0},.ffi=[](::vwa::VM *vm){::vwa::vm_int* p0;p0=vm->stack.pop<::vwa::vm_int*>();::core::print_raw(p0);},.ffi_direct=reinterpret_cast<void*>(::core::print_raw)}},
::vwa::Linker::Symbol{{"print_c_str","core"},::vwa::Linker::Symbol::Function{.type=::vwa::Linker::Symbol::Function::External,.params={{.type={{"char",""},1}},},.returnType={{"void",""},0},.ffi=[](::vwa::VM *vm){::vwa::vm_char* p0;p0=vm->stack.pop<::vwa::vm_char*>();::core::print_c_str(p0);},.ffi_direct=reinterpret_cast<void*>(::core::print_c_str)}},
::vwa::Linker::Symbol{{"getLine","core"},::vwa::Linker::Symbol::Function{.type=::vwa::Linker::Symbol::Function::External,.params={},.returnType={{"String","core"},0},.ffi=[](::vwa::VM *vm){vm->stack.push(::core::getLine());},.ffi_direct=reinterpret_cast<void*>(::core::getLine)}},
::vwa::Linker::Symbol{{"getLine_raw","core"},::vwa::Linker::Symbol::Function{.type=::vwa::Linker::Symbol::Function::External,.params={},.returnType={{"int",""},1},.ffi=[](::vwa::VM *vm){vm->stack.push(::core::getLine_raw());},.ffi_direct=reinterpret_cast<void*>(::core::getLine_raw)}},
::vwa::Linker::Symbol{{"malloc","core"},::vwa::Linker::Symbol::Function{.type=::vwa::Linker::Symbol::Function::External,.params={{.type={{"int",""},0}},},.returnType={{"void",""},1},.ffi=[](::vwa::VM *vm){::vwa::vm_int p0;p0=vm->stack.pop<::vwa::vm_int>();::core::malloc(p0);},.ffi_direct=reinterpret_cast<void*>(::core::malloc)}},
::vwa::Linker::Symbol{{"free","core"},::vwa::Linker::Symbol::Function{.type=::vwa::Linker::Symbol::Function::External,.params={{.type={{"void",""},1}},},.returnType={{"void",""},0},.ffi=[](::vwa::VM *vm){void* p0;p0=vm->stack.pop<void*>();::core::free(p0);},.ffi_direct=reinterpret_cast<void*>(::core::free)}},
::vwa::Linker::Symbol{{"realloc","core"},::vwa::Linker::Symbol::Function{.type=::vwa::Linker::Symbol::Function::External,.params={{.type={{"void",""},1}},{.type={{"int",""},0}},},.returnType={{"void",""},1},.ffi=[](::vwa::VM *vm){void* p0;::vwa::vm_int p1;p1=vm->stack.pop<::vwa::vm_int>();p0=vm->stack.pop<void*>();::core::realloc(p0,p1);},.ffi_direct=reinterpret_cast<void*>(::core::realloc)}},
::vwa::Linker::Symbol{{"memcpy","core"},::vwa::Linker::Symbol::Function{.type=::vwa::Linker::Symbol::Function::External,.params={{.type={{"void",""},1}},{.type={{"void",""},1}},{.type={{"int",""},0}},},.returnType={{"void",""},1},.ffi=[](::vwa::VM *vm){void* p0;void* p1;::vwa::vm_int p2;p2=vm->stack.pop<::vwa::vm_int>();p1=vm->stack.pop<void*>();p0=vm->stack.pop<void*>();::core::memcpy(p0,p1,p2);},.ffi_direct=reinterpret_cast<void*>(::core::memcpy)}},
::vwa::Linker::Symbol{{"memmove","core"},::vwa::Linker::Symbol::Function{.type=::vwa::Linker::Symbol::Function::External,.params={{.type={{"void",""},1}},{.type={{"void",""},1}},{.type={{"int",""},0}},},.returnType={{"void",""},1},.ffi=[](::vwa::VM *vm){void* p0;void* p1;::vwa::vm_int p2;p2=vm->stack.pop<::vwa::vm_int>();p1=vm->stack.pop<void*>();p0=vm->stack.pop<void*>();::core::memmove(p0,p1,p2);},.ffi_direct=reinterpret_cast<void*>(::core::memmove)}},
::vwa::Linker::Symbol{{"calloc","core"},::vwa::Linker::Symbol::Function{.type=::vwa::Linker::Symbol::Function::External,.params={{.type={{"int",""},0}},{.type={{"int",""},0}},},.returnType={{"void",""},1},.ffi=[](::vwa::VM *vm){::vwa::vm_int p0;::vwa::vm_int p1;p1=vm->stack.pop<::vwa::vm_int>();p0=vm->stack.pop<::vwa::vm_int>();::core::calloc(p0,p1);},.ffi_direct=reinterpret_cast<void*>(::core::calloc)}},
}};
return module;
}
void InternalLink(::vwa::Linker&){
//TODO: fill in the unlinked requirements here
}
void InternalExit(){}
#ifndef MODULE_COSTUM_ENTRY_POINT
extern "C"{
::vwa::Linker::Module VM_ONLOAD(){return InternalLoad();}
void VM_ONLINK(::vwa::Linker&linker){InternalLink(linker);}
void VM_ONEXIT(){InternalExit();}
}
#endif
#endif
