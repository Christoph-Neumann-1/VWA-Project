add_library(data SHARED data.cpp)
target_link_libraries(data linker vmLib)
file(MAKE_DIRECTORY bin)

add_custom_command(TARGET data
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:data> ${CMAKE_CURRENT_SOURCE_DIR}/bin/data.native)