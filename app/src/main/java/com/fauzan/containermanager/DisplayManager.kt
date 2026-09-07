package com.fauzan.containermanager

import android.util.Log
import android.view.Surface

object DisplayManager {
    init {
        System.loadLibrary("containermanager")
        Log.i("CMDisplayMgr", "DisplayManager: starting display (Build Version: 2026-09-06 v2)")
    }

    @JvmStatic external fun nativeStartComposerService()
    @JvmStatic external fun nativeSurfaceCreated(displayId: Long, surface: Surface)
    @JvmStatic external fun nativeSurfaceChanged(displayId: Long, surface: Surface, dpi: Int, refresh: Float)
    @JvmStatic external fun nativeSurfaceDestroyed(displayId: Long, surface: Surface)
    @JvmStatic external fun nativeDisplayDestroyed(displayId: Long)
    @JvmStatic external fun nativeGetUiRunning(): Boolean
    @JvmStatic external fun nativeSetAppForeground(displayId: Long, foreground: Boolean)
    @JvmStatic external fun nativeInitInputDevice()
    @JvmStatic external fun nativeReconfigureInputDevice(displayId: Long, width: Int, height: Int)
    @JvmStatic external fun nativeStopInputDevice(displayId: Long)

    private var isComposerStarted = false

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
        if (!isComposerStarted) {
            nativeStartComposerService()
            nativeInitInputDevice()
            isComposerStarted = true
        }
        
        val display = if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            context.display
        } else {
            @Suppress("DEPRECATION")
            val wm = context.getSystemService(android.content.Context.WINDOW_SERVICE) as android.view.WindowManager
            wm.defaultDisplay
        }
        val refreshRate = display?.refreshRate ?: 60f
        val dpi = context.resources.configuration.densityDpi
        
        nativeSurfaceCreated(0L, surface)
        nativeSurfaceChanged(0L, surface, dpi, refreshRate)
        nativeSetAppForeground(0L, true)
    }

    fun stopDisplay(surface: Surface) {
        if (isComposerStarted) {
            nativeSetAppForeground(0L, false)
            nativeSurfaceDestroyed(0L, surface)
            nativeDisplayDestroyed(0L)
            nativeStopInputDevice(0L)
        }
    }
}
