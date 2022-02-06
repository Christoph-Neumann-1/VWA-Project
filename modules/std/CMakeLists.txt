add_library(std SHARED Main.cpp)
target_link_libraries(std linker vmLib)
file(MAKE_DIRECTORY bin)

add_custom_command(TARGET std
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:std> ${CMAKE_CURRENT_SOURCE_DIR}/bin/std.native)