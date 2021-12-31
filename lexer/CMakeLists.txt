file(GLOB_RECURSE SOURCES "src/*.cpp")
add_library(lexer STATIC ${SOURCES})
target_include_directories(lexer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)