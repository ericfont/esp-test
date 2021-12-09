idf_component_register(
    SRCS "esp32_aoo_test_main.c" "ethernet_basic.c"
    INCLUDE_DIRS "aoo/include" "aoo/deps")

# disable warning in Opus library treated as error
add_compile_options("-Wno-error=maybe-uninitialized")

add_subdirectory(aoo EXCLUDE_FROM_ALL)

target_link_libraries(${COMPONENT_LIB} PUBLIC AOO_STATIC)
