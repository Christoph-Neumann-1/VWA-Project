cmake_minimum_required(VERSION 3.18.0) #Why not
project(ProgrammingLanguage)

enable_testing()
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(extern/CLI11)
add_subdirectory(extern/Catch2)
include(extern/Catch2/extras/Catch.cmake)
add_compile_options(-fPIC)#Fmt doesn't like shared libraries
add_subdirectory(extern/fmt)#TODO: write a wrapper around this to support redirecting to files or find out how to do that using normal library functions

# add_compile_options(-Wall -Wextra -Wno-unknown-pragmas) #-fconcepts-diagnostics-depth=6)
add_compile_options(-w)
add_subdirectory(common)
link_libraries(common fmt::fmt)
add_subdirectory(preprocessor)#This is seperated out because both the linker and the compiler require it
add_subdirectory(lexer)
add_subdirectory(linker)
add_subdirectory(AST)
add_subdirectory(vm)
add_subdirectory(compiler)
add_subdirectory(modules)
add_subdirectory(test)
add_subdirectory(BytecodeTools)

add_custom_target(Basic DEPENDS compiler vm standardLib)