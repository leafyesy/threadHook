package com.ysydhc.threadhook

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import com.tencent.threadhook.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val threadHook = ThreadHook()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.start.setOnClickListener {
            MySdk.init()
            threadHook.threadHook()
        }
        binding.setThreadStack.setOnClickListener {
            threadHook.setNativeThreadStackSize(100L, ThreadSizeCallback())
        }
        binding.startThread.setOnClickListener {
            Thread("test_hook").start()
        }
    }

    companion object {
        init {
            System.loadLibrary("threadhook")
        }
    }
}