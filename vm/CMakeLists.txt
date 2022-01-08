file(GLOB_RECURSE SOURCES "src/*.cpp")
add_library(vmLib STATIC ${SOURCES})#For testing purposes this is a library. The executable then calls into that library
target_link_libraries(vmLib linker)
target_include_directories(vmLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)