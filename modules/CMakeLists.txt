#TODO: auto detect

add_subdirectory(core)
add_subdirectory(data)

add_custom_target(standardLib DEPENDS core data)

add_executable(boilerplate GenerateBoilerplate.cpp)
target_link_libraries(boilerplate linker)