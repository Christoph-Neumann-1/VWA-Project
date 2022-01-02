# file(GLOB_RECURSE SOURCES "src/*.cpp")
# add_library(compilerLib STATIC ${SOURCES})#For testing purposes this is a library. The executable then calls into that library
# target_link_libraries(compilerLib preprocessor linker vm)
add_executable(compiler main.cpp)
# target_link_libraries(compiler compilerLib)
target_link_libraries(compiler preprocessor lexer ast CLI11::CLI11)#This will get removed when I finish the preprocessor

