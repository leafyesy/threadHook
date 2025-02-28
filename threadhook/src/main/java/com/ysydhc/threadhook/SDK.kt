package com.ysydhc.threadhook

import android.content.Context
import com.bytedance.shadowhook.ShadowHook
import com.bytedance.shadowhook.ShadowHook.ConfigBuilder

object MySdk {

    fun init(context: Context) {
        ShadowHook.init(
            ConfigBuilder().setMode(ShadowHook.Mode.UNIQUE).build()
        )
    }

}