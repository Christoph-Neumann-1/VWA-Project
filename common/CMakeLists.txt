file(GLOB_RECURSE SOURCES "src/*.cpp")
add_library(common STATIC ${SOURCES})#For testing purposes this is a library. The executable then calls into that library
target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
