package com.fauzan.containermanager

import android.util.Log
import android.view.Surface

object DisplayManager {
    init {
        System.loadLibrary("containermanager")
        Log.i("CMDisplayMgr", "DisplayManager: starting display (Build Version: 2026-09-06 v2)")
    }

    private var handle: Long = 0

    @JvmStatic external fun nativeCreate(): Long
    @JvmStatic external fun nativeDestroy(handle: Long)
    @JvmStatic external fun nativeConfigure(
        handle: Long, socketPath: String, useRoot: Boolean,
        helperPath: String, bridgePath: String, topappEnable: Boolean, topappPath: String,
        topappMode: Int, topappStops: String
    )
    @JvmStatic external fun nativeSetRefreshRate(handle: Long, hz: Float)
    @JvmStatic external fun nativeStart(handle: Long, surface: Surface, clipboardTarget: Any?, activityTarget: Any?)
    @JvmStatic external fun nativeStop(handle: Long)

    @JvmStatic
    fun runRootCommandAsync(command: String) {
        Thread {
            try {
                val result = com.topjohnwu.superuser.Shell.cmd(command).exec()
                if (!result.isSuccess) {
                    Log.e("CMDisplayMgr", "Root command failed. code=${result.code}")
                    for (err in result.err) {
                        Log.e("CMDisplayMgr", "stderr: $err")
                    }
                }
                for (out in result.out) {
                    Log.i("CMDisplayMgr", "stdout: $out")
                }
            } catch (e: Exception) {
                Log.e("CMDisplayMgr", "Error executing root command", e)
            }
        }.start()
    }

    fun startDisplay(context: android.content.Context, surface: Surface, containerPath: String) {
        if (handle == 0L) {
            handle = nativeCreate()
        }
        val socketPath = "$containerPath/var/display_daemon.sock"
        val bridgePath = "${context.cacheDir.absolutePath}/bridge.sock"
        val helperPath = "${context.applicationInfo.nativeLibraryDir}/libfdhelper.so"
        nativeConfigure(handle, socketPath, true, helperPath, bridgePath, false, "", 1, "")
        
        val display = if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            context.display
        } else {
            @Suppress("DEPRECATION")
            val wm = context.getSystemService(android.content.Context.WINDOW_SERVICE) as android.view.WindowManager
            wm.defaultDisplay
        }
        val refreshRate = display?.refreshRate ?: 60f
        nativeSetRefreshRate(handle, refreshRate)
        
        nativeStart(handle, surface, null, null)
    }

    fun stopDisplay() {
        if (handle != 0L) {
            nativeStop(handle)
        }
    }
}
