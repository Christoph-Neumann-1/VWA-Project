#TODO: auto detect

add_subdirectory(std)

add_executable(boilerplate GenerateBoilerplate.cpp)
target_link_libraries(boilerplate linker)