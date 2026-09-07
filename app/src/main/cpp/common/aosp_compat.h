#pragma once

#include <android/log.h>
#include <android/hardware_buffer.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdlib.h>
#include <vector>

// Provide ALOGE, ALOGI
#ifndef ALOG_TAG
#define ALOG_TAG "AOSP_COMPAT"
#endif

#ifndef ALOGE
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, ALOG_TAG, __VA_ARGS__)
#endif
#ifndef ALOGI
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, ALOG_TAG, __VA_ARGS__)
#endif
#ifndef ALOGD
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, ALOG_TAG, __VA_ARGS__)
#endif
#ifndef ALOGW
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, ALOG_TAG, __VA_ARGS__)
#endif

#define NO_ERROR 0
#define OK 0
#define NO_INIT -ENODEV
#define BAD_VALUE -EINVAL

typedef int32_t status_t;

// Provide native_handle_t
typedef struct native_handle {
    int version;
    int numFds;
    int numInts;
    int data[0];
} native_handle_t;

inline native_handle_t* native_handle_create(int numFds, int numInts) {
    size_t mallocSize = sizeof(native_handle_t) + (sizeof(int) * (numFds + numInts));
    native_handle_t* h = (native_handle_t*) malloc(mallocSize);
    if (h) {
        h->version = sizeof(native_handle_t);
        h->numFds = numFds;
        h->numInts = numInts;
    }
    return h;
}

inline int native_handle_close(const native_handle_t* h) {
    if (!h) return 0;
    int saved_errno = errno;
    for (int i = 0; i < h->numFds; ++i) {
        close(h->data[i]);
    }
    errno = saved_errno;
    return 0;
}

inline int native_handle_delete(native_handle_t* h) {
    if (h) {
        free(h);
    }
    return 0;
}

// Convert AIDL NativeHandle to native_handle_t
namespace android {
    template <typename T>
    inline native_handle_t* makeFromAidl(const T& handle) {
        native_handle_t* nh = native_handle_create(handle.fds.size(), handle.ints.size());
        if (!nh) return nullptr;
        for (size_t i = 0; i < handle.fds.size(); i++) {
            nh->data[i] = dup(handle.fds[i].get());
        }
        for (size_t i = 0; i < handle.ints.size(); i++) {
            nh->data[handle.fds.size() + i] = handle.ints[i];
        }
        return nh;
    }
    
    inline void destroy_cloned_handle(native_handle_t* h) {
        native_handle_close(h);
        native_handle_delete(h);
    }
    
    // Load AHardwareBuffer_createFromHandle via dlsym
    inline int32_t AHardwareBuffer_createFromHandle(
        const AHardwareBuffer_Desc* desc,
        const native_handle_t* handle,
        int32_t method,
        AHardwareBuffer** outBuffer) {
        
        typedef int32_t (*AHB_createFromHandle_t)(const AHardwareBuffer_Desc*, const native_handle_t*, int32_t, AHardwareBuffer**);
        static AHB_createFromHandle_t func = nullptr;
        if (!func) {
            void* lib = dlopen("libnativewindow.so", RTLD_NOW);
            if (lib) {
                func = (AHB_createFromHandle_t) dlsym(lib, "AHardwareBuffer_createFromHandle");
            }
        }
        if (func) {
            return func(desc, handle, method, outBuffer);
        }
        ALOGE("Failed to dlsym AHardwareBuffer_createFromHandle");
        return -1; // NO_ERROR is 0
    }
}

#define AHARDWAREBUFFER_CREATE_FROM_HANDLE_METHOD_CLONE 2
