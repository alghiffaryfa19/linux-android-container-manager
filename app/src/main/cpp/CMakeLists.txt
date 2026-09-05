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

# Copy the executable to the assets folder so Gradle can package it into the APK
add_custom_command(TARGET container_boot POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/../assets/${ANDROID_ABI}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:container_boot> ${CMAKE_SOURCE_DIR}/../assets/${ANDROID_ABI}/container_boot)
