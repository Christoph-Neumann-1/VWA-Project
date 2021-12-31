cmake_minimum_required(VERSION 3.18.0) #Why not
project(ProgrammingLanguage)
enable_testing()
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(preprocessor)#This is seperated out because both the linker and the compiler require it
add_subdirectory(lexer)
add_subdirectory(linker)
add_subdirectory(vm)
add_subdirectory(extern/CLI11)
add_subdirectory(compiler)
add_subdirectory(test)
add_subdirectory(modules)

add_custom_target(complete DEPENDS compiler vm stdlib)