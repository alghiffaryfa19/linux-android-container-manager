#define ALOG_TAG "ContainerManagerNative"

#include <jni.h>
#include <string>

extern "C" {
    typedef int32_t binder_status_t;
    struct AIBinder;
    binder_status_t AServiceManager_addService(AIBinder* binder, const char* instance);
    void ABinderProcess_joinThreadPool();
}
#include <android/native_window_jni.h>
#include "aosp_compat.h"
#include <memory>

#include "ComposerImpl.h"
#include "InputDevice.h"

using aidl::vendor::lindroid::composer::ComposerImpl;
using namespace android;

static std::shared_ptr<ComposerImpl> composer = nullptr;
static std::shared_ptr<InputDevice> inputDevice = nullptr;

extern "C" void
Java_com_fauzan_containermanager_DisplayManager_nativeStartComposerService(
    JNIEnv *env, jclass /* clazz */) {
    ALOGI("Init native: Starting composer binder service...");

    composer = ndk::SharedRefBase::make<ComposerImpl>();
    binder_status_t status = AServiceManager_addService(composer->asBinder().get(), "vendor.lindroid.composer");
    if (status != STATUS_OK) {
        ALOGE("Could not register composer binder service");
    }
    ABinderProcess_joinThreadPool();
}

extern "C" void
Java_com_fauzan_containermanager_DisplayManager_nativeSurfaceCreated(
    JNIEnv *env, jclass /* clazz */,
    jlong displayId, jobject surface) {
    ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (nativeWindow == nullptr) {
        ALOGE("Get ANativeWindow ERROR!");
        return;
    }
    if (composer == nullptr) return;
    composer->onSurfaceCreated(displayId, nativeWindow);
}

extern "C" void
Java_com_fauzan_containermanager_DisplayManager_nativeSurfaceChanged(
    JNIEnv *env, jclass /* clazz */,
    jlong displayId, jobject surface, jint dpi, jfloat refresh) {
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (nativeWindow == nullptr) return;
    if (composer == nullptr) return;
    composer->onSurfaceChanged(displayId, nativeWindow, dpi, refresh);
}

extern "C" void
Java_com_fauzan_containermanager_DisplayManager_nativeSurfaceDestroyed(
    JNIEnv *env, jclass /* clazz */,
    jlong displayId, jobject surface) {
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (nativeWindow == nullptr) return;
    if (composer == nullptr) return;
    composer->onSurfaceDestroyed(displayId, nativeWindow);
}

extern "C" void
Java_com_fauzan_containermanager_DisplayManager_nativeDisplayDestroyed(
    JNIEnv *env, jclass /* clazz */,
    jlong displayId) {
    if (composer == nullptr) return;
    composer->onDisplayDestroyed(displayId);
}

extern "C" jboolean
Java_com_fauzan_containermanager_DisplayManager_nativeGetUiRunning(
    JNIEnv *env, jclass /* clazz */) {
    if (composer == nullptr) return JNI_FALSE;
    bool isUiRunning;
    composer->getUiRunning(&isUiRunning);
    return isUiRunning;
}

extern "C" void
Java_com_fauzan_containermanager_DisplayManager_nativeSetAppForeground(
    JNIEnv *env, jclass /* clazz */,
    jlong displayId, jboolean foreground) {
    if (composer == nullptr) return;
    composer->onAppForegroundChanged(displayId, foreground);
}

extern "C" void
Java_com_fauzan_containermanager_DisplayManager_nativeInitInputDevice(
    JNIEnv *env, jclass /* clazz */) {
    inputDevice = std::make_shared<InputDevice>();
}

extern "C" void
Java_com_fauzan_containermanager_DisplayManager_nativeReconfigureInputDevice(
    JNIEnv *env, jclass /* clazz */,
    jlong displayId, jint width, jint height) {
    if (inputDevice == nullptr) return;
    inputDevice->reconfigure(displayId, width, height);
}

extern "C" void
Java_com_fauzan_containermanager_DisplayManager_nativeStopInputDevice(
    JNIEnv *env, jclass /* clazz */,
    jlong displayId) {
    if (inputDevice == nullptr) return;
    inputDevice->stop(displayId);
}
