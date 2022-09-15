add_library(core SHARED core.cpp)
target_link_libraries(core linker vmLib)
file(MAKE_DIRECTORY bin)

add_custom_command(TARGET core
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:core> ${CMAKE_CURRENT_SOURCE_DIR}/bin/core.native)