#TODO: auto detect

add_subdirectory(core)

add_executable(boilerplate GenerateBoilerplate.cpp)
target_link_libraries(boilerplate linker)