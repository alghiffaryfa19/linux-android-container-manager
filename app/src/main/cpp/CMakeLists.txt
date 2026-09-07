cmake_minimum_required(VERSION 3.22.1)

project("containermanager" C CXX)

set(AIDL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/interfaces/composer)
file(GLOB_RECURSE AIDL_FILES "${AIDL_DIR}/*.aidl")

set(AIDL_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/aidl_out)
file(MAKE_DIRECTORY ${AIDL_OUT_DIR})

find_program(AIDL_EXECUTABLE NAMES aidl PATHS "$ENV{ANDROID_SDK_ROOT}/build-tools" "$ENV{ANDROID_HOME}/build-tools" PATH_SUFFIXES "34.0.0" "33.0.2" "33.0.1" "33.0.0" "32.0.0" "31.0.0" "30.0.3")

if(AIDL_EXECUTABLE)
    set(AIDL_CPP_SRCS "")
    foreach(aidl_file ${AIDL_FILES})
        file(RELATIVE_PATH aidl_rel ${AIDL_DIR} ${aidl_file})
        string(REPLACE ".aidl" "" aidl_base ${aidl_rel})
        set(out_cpp ${AIDL_OUT_DIR}/${aidl_base}.cpp)
        set(out_h ${AIDL_OUT_DIR}/${aidl_base}.h)
        # NDK AIDL also generates an additional directory prefix `aidl/` for headers!
        set(out_h_real ${AIDL_OUT_DIR}/aidl/${aidl_base}.h)
        add_custom_command(
            OUTPUT ${out_cpp} ${out_h_real}
            COMMAND ${AIDL_EXECUTABLE} --lang=ndk -I${AIDL_DIR} -o${AIDL_OUT_DIR} -h${AIDL_OUT_DIR} ${aidl_file}
            DEPENDS ${aidl_file}
        )
        list(APPEND AIDL_CPP_SRCS ${out_cpp})
    endforeach()
    add_custom_target(generate_aidl_files DEPENDS ${AIDL_CPP_SRCS})
else()
    message(WARNING "aidl tool not found! Ensure build-tools is installed.")
endif()
add_library(
        containermanager
        SHARED
        native_consumer.cpp
        ComposerImpl.cpp
        InputDevice.cpp
        ${AIDL_CPP_SRCS}
        libdisplay_consumer/display_consumer.c
        common/socket_utils.c)

target_include_directories(containermanager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/libdisplay_consumer
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${AIDL_OUT_DIR}
)

if(AIDL_EXECUTABLE)
    add_dependencies(containermanager generate_aidl_files)
endif()

# Note: We omit audio, camera, tracy etc. to keep it lightweight.

target_link_libraries(
        containermanager
        android
        log
        dl
        binder_ndk)

# Add the standalone boot binary executable
add_executable(container_boot boot.c)
set_target_properties(container_boot PROPERTIES PREFIX "lib" SUFFIX ".so")

# Add the fdhelper executable
add_executable(fdhelper fd_helper.c common/socket_utils.c)
target_include_directories(fdhelper PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_link_libraries(fdhelper log)
set_target_properties(fdhelper PROPERTIES PREFIX "lib" SUFFIX ".so")
