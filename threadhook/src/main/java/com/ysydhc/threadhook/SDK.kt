package com.ysydhc.threadhook

import com.bytedance.shadowhook.ShadowHook
import com.bytedance.shadowhook.ShadowHook.ConfigBuilder


object MySdk {
    fun init() {
        ShadowHook.init(
            ConfigBuilder()
                    .setMode(ShadowHook.Mode.UNIQUE)
                    .build()
        )
        StackPrinter.print()
    }
}