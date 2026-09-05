package com.fauzan.containermanager

import android.view.Surface

object DisplayManager {
    init {
        System.loadLibrary("containermanager")
    }

    private var handle: Long = 0

    @JvmStatic external fun nativeCreate(): Long
    @JvmStatic external fun nativeDestroy(handle: Long)
    @JvmStatic external fun nativeStart(handle: Long, surface: Surface, clipboardTarget: Any?, activityTarget: Any?)
    @JvmStatic external fun nativeStop(handle: Long)

    fun startDisplay(surface: Surface) {
        if (handle == 0L) {
            handle = nativeCreate()
        }
        nativeStart(handle, surface, null, null)
    }

    fun stopDisplay() {
        if (handle != 0L) {
            nativeStop(handle)
        }
    }
}
