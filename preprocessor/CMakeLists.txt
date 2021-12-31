file(GLOB_RECURSE SOURCES "src/*.cpp")
add_library(preprocessor STATIC ${SOURCES})
target_include_directories(preprocessor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
