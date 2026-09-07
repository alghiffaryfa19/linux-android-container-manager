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
#include "common/socket_msg.h"

namespace aidl {
namespace vendor {
namespace lindroid {
namespace composer {

struct DisplayConfiguration {
    int64_t displayId;
    uint32_t width;
    uint32_t height;
    struct {
        int32_t x;
        int32_t y;
    } dpi;
    int64_t vsyncPeriod;
    int32_t configId;
};

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

class ComposerImpl {
public:
    void setBufferFromFd(int64_t displayId, int fd, uint32_t width, uint32_t height, uint32_t stride, uint32_t format);

    void onSurfaceCreated(int64_t displayId, ANativeWindow *nativeWindow);
    void onSurfaceChanged(int64_t displayId, ANativeWindow *nativeWindow, int dpi, float refresh);
    void onSurfaceDestroyed(int64_t displayId, ANativeWindow *nativeWindow);
    void onDisplayDestroyed(int64_t displayId);
    void onAppForegroundChanged(int64_t displayId, bool foreground);

    void getUiRunning(bool* isUiRunning);

private:
    std::mutex mLock;

    std::unordered_map<int64_t, ComposerDisplay*> mDisplays;
    
    bool m_ui_running = false;
};

} // namespace composer
} // namespace lindroid
} // namespace vendor
} // namespace aidl
