/*
 * uEventPatch — Xposed hook for Lindroid ueventd compatibility
 *
 * Hooks ExtconStateObserver.onUEvent() and WiredAccessoryManager$WiredAccessoryObserver.onUEvent()
 * to skip UEvents with null NAME, preventing the "Error reading from Uevent Fd: I/O error" crash
 * that occurs when running Linux containers via Lindroid.
 *
 * Based on: https://github.com/alghiffaryfa19/uEventPatch
 * Original author: fish4terrisa-MSDSM <flyingfish.msdsm@gmail.com>
 * License: GPL-3.0
 */
package com.fauzan.containermanager

import de.robv.android.xposed.IXposedHookLoadPackage
import de.robv.android.xposed.XC_MethodHook
import de.robv.android.xposed.XposedBridge
import de.robv.android.xposed.XposedHelpers
import de.robv.android.xposed.callbacks.XC_LoadPackage

class UEventPatchHook : IXposedHookLoadPackage {

    companion object {
        private const val TAG = "uEventPatch"
    }

    override fun handleLoadPackage(lpparam: XC_LoadPackage.LoadPackageParam) {
        if (lpparam.packageName != "android") return

        XposedBridge.log("D/$TAG Loaded")
        hookUEvents(lpparam)
    }

    private fun hookUEvents(lpparam: XC_LoadPackage.LoadPackageParam) {
        // Hook ExtconStateObserver.onUEvent() — skip if UEvent NAME is null
        XposedHelpers.findAndHookMethod(
            "com.android.server.ExtconStateObserver",
            lpparam.classLoader,
            "onUEvent",
            "com.android.server.ExtconUEventObserver\$ExtconInfo",
            "android.os.UEventObserver\$UEvent",
            object : XC_MethodHook() {
                override fun beforeHookedMethod(param: MethodHookParam) {
                    val name = XposedHelpers.callMethod(param.args[1], "get", "NAME") as? String
                    if (name == null) {
                        XposedBridge.log("D/$TAG Removed NAME (ExtconStateObserver)")
                        param.result = null
                    }
                }
            }
        )

        // Hook WiredAccessoryManager$WiredAccessoryObserver.onUEvent() — skip if both NAME and SWITCH_PATH are null
        XposedHelpers.findAndHookMethod(
            "com.android.server.WiredAccessoryManager\$WiredAccessoryObserver",
            lpparam.classLoader,
            "onUEvent",
            "android.os.UEventObserver\$UEvent",
            object : XC_MethodHook() {
                override fun beforeHookedMethod(param: MethodHookParam) {
                    val name = XposedHelpers.callMethod(param.args[0], "get", "NAME") as? String
                    if (name == null) {
                        val switchPath = XposedHelpers.callMethod(param.args[0], "get", "SWITCH_PATH") as? String
                        if (switchPath == null) {
                            XposedBridge.log("D/$TAG Removed SWITCH_PATH (WiredAccessoryObserver)")
                            param.result = null
                        }
                    }
                }
            }
        )
    }
}
