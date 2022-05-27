file(GLOB_RECURSE SOURCES "src/*.cpp")
add_library(linker STATIC ${SOURCES})#For testing purposes this is a library. The executable then calls into that library
target_link_libraries(linker vmLib dl)
target_include_directories(linker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)