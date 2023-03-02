add_custom_command(OUTPUT core.interface COMMAND $<TARGET_FILE:boilerplate> -i ${CMAKE_CURRENT_SOURCE_DIR}/core.template DEPENDS boilerplate core.template WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_custom_target(core_interface ALL SOURCES core.interface)
add_custom_command(OUTPUT core.gen.hpp COMMAND $<TARGET_FILE:boilerplate> ${CMAKE_CURRENT_SOURCE_DIR}/core.template DEPENDS boilerplate core_interface WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_custom_target(core_header ALL DEPENDS core.gen.hpp)

add_library(core SHARED core.cpp)
add_dependencies(core core_header)
target_link_libraries(core linker vmLib)
file(MAKE_DIRECTORY bin)

add_custom_command(TARGET core
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:core> ${CMAKE_CURRENT_SOURCE_DIR}/bin/core.native)