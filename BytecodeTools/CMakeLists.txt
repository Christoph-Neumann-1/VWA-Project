add_executable(disassembler src/Disassembler.cpp)
add_executable(assembler src/Assembler.cpp)

target_include_directories(disassembler PRIVATE include)
target_include_directories(assembler PRIVATE include)

target_link_libraries(disassembler PRIVATE vmLib)
target_link_libraries(assembler PRIVATE vmLib)
