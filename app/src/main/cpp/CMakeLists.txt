cmake_minimum_required(VERSION 3.22.1)

project("containermanager" C CXX)

add_library(
        containermanager
        SHARED
        native_consumer.c
        libdisplay_consumer/display_consumer.c
        common/socket_utils.c)

target_include_directories(containermanager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/libdisplay_consumer
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

# Note: We omit audio, camera, tracy etc. to keep it lightweight.

target_link_libraries(
        containermanager
        android
        log
        dl)

# Add the standalone boot binary executable
add_executable(container_boot boot.c)

# Make the executable look like a shared library so Gradle packages it automatically
set_target_properties(container_boot PROPERTIES PREFIX "lib" SUFFIX ".so")
