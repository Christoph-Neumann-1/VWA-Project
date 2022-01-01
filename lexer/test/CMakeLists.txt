file(GLOB_RECURSE TEST_SRC "src/*.cpp")
add_executable(testLexer ${TEST_SRC})#If I make lots of tests it might be worth splitting them into multiple targets
target_link_libraries(testLexer Catch2::Catch2WithMain lexer)
catch_discover_tests(testLexer)
#I might want to use precompiled headers shared between all the tests