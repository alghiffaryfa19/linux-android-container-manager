#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <android/surface_control.h>
#include <android/native_window.h>

#include <aidl/android/hardware/graphics/common/HardwareBuffer.h>
#include <aidl/vendor/lindroid/composer/BnComposer.h>
#include <aidl/vendor/lindroid/composer/DisplayConfiguration.h>
#include <aidl/vendor/lindroid/composer/IComposerCallback.h>

using aidl::android::hardware::graphics::common::HardwareBuffer;
using aidl::vendor::lindroid::composer::DisplayConfiguration;
using aidl::vendor::lindroid::composer::IComposerCallback;

namespace aidl {
namespace vendor {
namespace lindroid {
namespace composer {

typedef std::function<void(int64_t /*timestampNs*/, uint32_t /*count32*/)> vsync_callback_t;

class VsyncThread {
public:
    void start(int64_t first, int64_t period);
    void stop();
    void setCallback(const vsync_callback_t &callback);

private:
    void vsyncLoop();
    std::thread mThread;
    std::mutex mMutex;
    bool mStarted{false};
    vsync_callback_t mCallback;
};

struct ComposerDisplay {
    ANativeWindow *nativeWindow = nullptr;
    ASurfaceControl *surfaceControl = nullptr;
    DisplayConfiguration displayConfig{};
    bool plugged = false;
    VsyncThread mVsyncThread;
    std::mutex mFenceLock;
    int mPresentFenceFd{-1};
};

class ComposerImpl : public BnComposer {
public:
    virtual ndk::ScopedAStatus registerCallback(const std::shared_ptr<IComposerCallback> &in_cb, int32_t sequenceId) override;
    virtual ndk::ScopedAStatus onHotplug(int64_t in_displayId, bool in_connected) override;
    virtual ndk::ScopedAStatus requestDisplay(int64_t in_displayId) override;
    virtual ndk::ScopedAStatus getActiveConfig(int64_t in_displayId, DisplayConfiguration *_aidl_return) override;
    virtual ndk::ScopedAStatus acceptChanges(int64_t in_displayId) override;
    virtual ndk::ScopedAStatus getReleaseFence(int64_t in_displayId, ndk::ScopedFileDescriptor *_aidl_return) override;
    virtual ndk::ScopedAStatus present(int64_t in_displayId, ndk::ScopedFileDescriptor *_aidl_return) override;
    virtual ndk::ScopedAStatus setPowerMode(int64_t in_displayId, int32_t in_mode) override;
    virtual ndk::ScopedAStatus setVsyncEnabled(int64_t in_displayId, int32_t in_enabled) override;
    virtual ndk::ScopedAStatus setBuffer(int64_t in_displayId, const HardwareBuffer &in_buffer, const ::ndk::ScopedFileDescriptor &in_fenceFd, int32_t *_aidl_return) override;
    virtual ndk::ScopedAStatus getUiRunning(bool *_aidl_return) override;

    void onSurfaceCreated(int64_t displayId, ANativeWindow *nativeWindow);
    void onSurfaceChanged(int64_t displayId, ANativeWindow *nativeWindow, int dpi, float refresh);
    void onSurfaceDestroyed(int64_t displayId, ANativeWindow *nativeWindow);
    void onDisplayDestroyed(int64_t displayId);
    void onAppForegroundChanged(int64_t displayId, bool foreground);

private:
    std::mutex mLock;

    int32_t mSequenceId;
    std::shared_ptr<IComposerCallback> mCallbacks;
    std::unordered_map<int64_t, ComposerDisplay*> mDisplays;
    
    bool m_ui_running = false;
};

} // namespace composer
} // namespace lindroid
} // namespace vendor
} // namespace aidl
