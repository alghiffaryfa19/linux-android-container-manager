package com.fauzan.containermanager

import android.view.Surface

object DisplayManager {
    init {
        System.loadLibrary("containermanager")
    }

    private var handle: Long = 0

    @JvmStatic external fun nativeCreate(): Long
    @JvmStatic external fun nativeDestroy(handle: Long)
    @JvmStatic external fun nativeConfigure(
        handle: Long, socketPath: String, useRoot: Boolean,
        helperPath: String, bridgePath: String, topappEnable: Boolean, topappPath: String,
        topappMode: Int, topappStops: String
    )
    @JvmStatic external fun nativeStart(handle: Long, surface: Surface, clipboardTarget: Any?, activityTarget: Any?)
    @JvmStatic external fun nativeStop(handle: Long)

    fun startDisplay(context: android.content.Context, surface: Surface, containerPath: String) {
        if (handle == 0L) {
            handle = nativeCreate()
        }
        val socketPath = "$containerPath/tmp/display_daemon.sock"
        val bridgePath = "$containerPath/tmp/bridge.sock"
        val helperPath = "${context.applicationInfo.nativeLibraryDir}/libfdhelper.so"
        nativeConfigure(handle, socketPath, true, helperPath, bridgePath, false, "", 1, "")
        nativeStart(handle, surface, null, null)
    }

    fun stopDisplay() {
        if (handle != 0L) {
            nativeStop(handle)
        }
    }
}
