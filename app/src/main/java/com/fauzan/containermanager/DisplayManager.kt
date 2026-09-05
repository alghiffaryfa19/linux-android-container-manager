package com.fauzan.containermanager

import android.view.Surface

object DisplayManager {
    init {
        System.loadLibrary("containermanager")
    }

    external fun startDisplay(surface: Surface)
    external fun stopDisplay()
}
