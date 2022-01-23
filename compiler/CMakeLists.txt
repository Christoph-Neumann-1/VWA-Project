file(GLOB_RECURSE SOURCES "src/*.cpp")
add_library(compilerLib STATIC ${SOURCES})#For testing purposes this is a library. The executable then calls into that library
target_link_libraries(compilerLib preprocessor linker vmLib ast linker)
target_include_directories(compilerLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/privateInclude)

add_executable(compiler main.cpp)
target_link_libraries(compiler compilerLib CLI11::CLI11)#This will get removed when I finish the preprocessor

