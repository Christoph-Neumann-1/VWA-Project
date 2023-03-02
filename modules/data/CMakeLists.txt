add_custom_command(OUTPUT data.interface COMMAND $<TARGET_FILE:boilerplate> -i ${CMAKE_CURRENT_SOURCE_DIR}/data.template DEPENDS boilerplate data.template WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_custom_target(data_interface ALL SOURCES data.interface)
add_custom_command(OUTPUT data.gen.hpp COMMAND $<TARGET_FILE:boilerplate> ${CMAKE_CURRENT_SOURCE_DIR}/data.template DEPENDS boilerplate DEPENDS data_interface core_interface WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_custom_target(data_header ALL DEPENDS data.gen.hpp)

add_library(data SHARED data.cpp)
add_dependencies(data data_header)
target_link_libraries(data linker vmLib)
file(MAKE_DIRECTORY bin)

add_custom_command(TARGET data
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:data> ${CMAKE_CURRENT_SOURCE_DIR}/bin/data.native)