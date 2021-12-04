idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS ".")

#set(AOO_BUILD_TEST_SUITE OFF CACHE BOOL "build test suite")
set(AOO_CODEC_OPUS OFF CACHE BOOL "use Opus codec")
#add_subdirectory(aoo)

add_library(aoo STATIC aoo)

target_link_libraries(${COMPONENT_LIB} PUBLIC aoo)

