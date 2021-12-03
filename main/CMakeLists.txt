idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS ".")

#option(AOO_BUILD_TEST_SUITE "build test suite" ON)
add_subdirectory(aoo)
