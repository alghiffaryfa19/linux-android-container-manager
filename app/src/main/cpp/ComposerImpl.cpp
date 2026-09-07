#include "ComposerImpl.h"
#include <android/hardware_buffer.h>
#include <android/log.h>
#include <unistd.h>
#include <cmath>
#include <cinttypes>

#define ALOG_TAG "ComposerImpl"
#include "common/aosp_compat.h"

namespace aidl {
namespace vendor {
namespace lindroid {
namespace composer {

void VsyncThread::start(int64_t first, int64_t period) {
    (void)first;
    (void)period;
    // Stubbed vsync thread
}

void VsyncThread::stop() {}
void VsyncThread::setCallback(const vsync_callback_t &callback) { mCallback = callback; }

static inline void close_if_valid(int fd) {
    if (fd >= 0) ::close(fd);
}

static inline float frame_rate_from_display_config(const DisplayConfiguration& cfg) {
    if (cfg.vsyncPeriod <= 0) return 60.0f;
    double hz = 1e9 / static_cast<double>(cfg.vsyncPeriod);
    if (!(hz > 1.0 && hz < 1000.0)) hz = 60.0;
    return static_cast<float>(hz);
}

void ComposerImpl::setBufferFromFd(int64_t displayId, int fd, uint32_t width, uint32_t height, uint32_t stride, uint32_t format) {
    ComposerDisplay* display = nullptr;
    {
        std::lock_guard<std::mutex> _l(mLock);
        if (!m_ui_running)
            m_ui_running = true;
        auto it = mDisplays.find(displayId);
        if (it == mDisplays.end() || it->second->nativeWindow == nullptr) {
            close_if_valid(fd);
            return;
        }
        display = it->second;
    }

    native_handle_t *nativeHandle = native_handle_create(1, 0);
    nativeHandle->data[0] = fd;

    const AHardwareBuffer_Desc desc{
        .width = width,
        .height = height,
        .layers = 1,
        .format = format,
        .usage = (1ULL << 8) | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, // GRALLOC_USAGE_HW_FB | GPU
        .stride = stride,
    };
    AHardwareBuffer *ahwb = nullptr;
    const status_t status = android::AHardwareBuffer_createFromHandle(
        &desc, nativeHandle, AHARDWAREBUFFER_CREATE_FROM_HANDLE_METHOD_CLONE, &ahwb);
    
    native_handle_delete(nativeHandle);
    
    if (status != 0) {
        ALOGE("%s: createFromHandle failed!", __FUNCTION__);
        close_if_valid(fd);
        return;
    }

    if (display->surfaceControl == nullptr) {
        ALOGE("%s: surfaceControl is null for display %" PRId64, __FUNCTION__, displayId);
        AHardwareBuffer_release(ahwb);
        close_if_valid(fd);
        return;
    }

    {
        std::lock_guard<std::mutex> _l(mLock);
        auto it = mDisplays.find(displayId);
        if (it == mDisplays.end() || it->second == nullptr || it->second->surfaceControl == nullptr) {
            AHardwareBuffer_release(ahwb);
            close_if_valid(fd);
            return;
        }
        display = it->second;
        ASurfaceControl* surfaceControl = display->surfaceControl;
        const float contentRate = frame_rate_from_display_config(display->displayConfig);

        ASurfaceTransaction* transaction = ASurfaceTransaction_create();

        ASurfaceTransaction_setOnComplete(transaction, display,
            [](void* ctx, ASurfaceTransactionStats* stats) {
                auto* d = static_cast<ComposerDisplay*>(ctx);
                if (!d) return;
                const int pfd = ASurfaceTransactionStats_getPresentFenceFd(stats);
                const int storeFd = pfd >= 0 ? ::dup(pfd) : -1;
                if (pfd >= 0) ::close(pfd);
                int oldFd = -1;
                {
                    std::lock_guard<std::mutex> lock(d->mFenceLock);
                    oldFd = d->mPresentFenceFd;
                    d->mPresentFenceFd = storeFd;
                }
                if (oldFd >= 0) ::close(oldFd);
            });

        ASurfaceTransaction_setFrameRateWithChangeStrategy(
            transaction,
            surfaceControl,
            contentRate,
            ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_FIXED_SOURCE,
            ANATIVEWINDOW_CHANGE_FRAME_RATE_ONLY_IF_SEAMLESS);

        // For simplicity we aren't using an acquire fence here
        ASurfaceTransaction_setBuffer(transaction, surfaceControl, ahwb, -1);
        ASurfaceTransaction_setVisibility(
            transaction, surfaceControl, ASURFACE_TRANSACTION_VISIBILITY_SHOW);

        ASurfaceTransaction_apply(transaction);
        ASurfaceTransaction_delete(transaction);
    }

    AHardwareBuffer_release(ahwb);
    close_if_valid(fd);
}

void ComposerImpl::getUiRunning(bool *isUiRunning) {
    *isUiRunning = m_ui_running;
}

void ComposerImpl::onSurfaceCreated(int64_t displayId, ANativeWindow *nativeWindow) {
    if (nativeWindow == nullptr) return;
    ALOGI("onSurfaceCreated Display: %" PRId64, displayId);
}

void ComposerImpl::onSurfaceChanged(int64_t displayId, ANativeWindow *nativeWindow, int dpi, float refresh) {
    if (nativeWindow == nullptr) return;
    ALOGI("onSurfaceChanged Display: %" PRId64, displayId);

    DisplayConfiguration displayConfig;
    displayConfig.displayId = displayId;
    displayConfig.width = ANativeWindow_getWidth(nativeWindow);
    displayConfig.height = ANativeWindow_getHeight(nativeWindow);
    displayConfig.dpi.x = dpi;
    displayConfig.dpi.y = dpi;
    double rate = static_cast<double>(refresh);
    if (!(rate > 1.0 && rate < 1000.0)) rate = 60.0;
    displayConfig.vsyncPeriod = static_cast<int64_t>(llround(1000000000.0 / rate));

    std::lock_guard<std::mutex> _l(mLock);

    ANativeWindow* previousNativeWindow = nullptr;
    ComposerDisplay* targetDisplay = nullptr;
    auto display = mDisplays.find(displayId);

    if (display != mDisplays.end() && display->second != nullptr) {
        targetDisplay = display->second;
        previousNativeWindow = targetDisplay->nativeWindow;
        targetDisplay->nativeWindow = nativeWindow;
        targetDisplay->displayConfig = displayConfig;
    } else {
        targetDisplay = new ComposerDisplay();
        targetDisplay->nativeWindow = nativeWindow;
        targetDisplay->displayConfig = displayConfig;
        mDisplays[displayId] = targetDisplay;
    }

    if (targetDisplay->surfaceControl == nullptr || previousNativeWindow != nativeWindow) {
        if (targetDisplay->surfaceControl) {
            ASurfaceControl_release(targetDisplay->surfaceControl);
            targetDisplay->surfaceControl = nullptr;
        }
        targetDisplay->surfaceControl = ASurfaceControl_createFromWindow(nativeWindow, "LindroidDisplay");
    }
}

void ComposerImpl::onSurfaceDestroyed(int64_t displayId, ANativeWindow *nativeWindow) {
    std::lock_guard<std::mutex> _l(mLock);
    auto it = mDisplays.find(displayId);
    if (it == mDisplays.end()) return;
    ComposerDisplay* display = it->second;
    display->nativeWindow = nullptr;
    if (display->surfaceControl) {
        ASurfaceControl_release(display->surfaceControl);
        display->surfaceControl = nullptr;
    }
}

void ComposerImpl::onDisplayDestroyed(int64_t displayId) {
    std::lock_guard<std::mutex> _l(mLock);
    auto it = mDisplays.find(displayId);
    if (it == mDisplays.end()) return;
    ComposerDisplay* display = it->second;
    if (display->surfaceControl) {
        ASurfaceControl_release(display->surfaceControl);
    }
    delete display;
    mDisplays.erase(it);
}

void ComposerImpl::onAppForegroundChanged(int64_t displayId, bool foreground) {}

} // namespace composer
} // namespace lindroid
} // namespace vendor
} // namespace aidl
