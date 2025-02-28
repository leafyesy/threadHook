package com.ysydhc.threadhook

import android.app.ActivityManager
import android.content.Context
import android.os.Debug
import android.util.Log
import java.io.BufferedReader
import java.io.FileReader

class ThreadInfoPrinter {

    companion object {
        private const val TAG = "ThreadInfoPrinter"
    }

    private val util = StackSizeUtil()

    fun printThreadInfo(context: Context) {
        val maxMemory = Runtime.getRuntime().maxMemory()
        Log.i("MySdk", "maxMemory: $maxMemory")

        val am = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        val memoryInfo = ActivityManager.MemoryInfo()
        am.getMemoryInfo(memoryInfo)
        val availMem = memoryInfo.availMem
        val totalMem = memoryInfo.totalMem
        Log.i("MySdk", "availMem: $availMem totalMem: $totalMem")

        val debugMemory = Debug.MemoryInfo()
        Debug.getMemoryInfo(debugMemory)
        val pssM = debugMemory.totalPss / 1024 // M
        Log.d("MemoryInfo", "应用实际占用内存 (PSS): $pssM MB")

        StackPrinter.print()
        val size = util.getThreadStackSize(Thread.currentThread().id)
        Log.i("MySdk", "threadSize: $size")
        Thread({
            Log.i("MySdk", "tid 100: " + util.tid)
            recursion(100) {
                Log.i("MySdk", "new threadSize: ${util.threadStackUsage}")
            }
        }, "mytest100").start()
        Thread({
            Log.i("MySdk", "tid 1000: " + util.tid)
            recursion(1000) {
                Log.i("MySdk", "new threadSize: ${util.threadStackUsage}")
            }
        }, "mytest1000").start()
        Thread({
            try {
                Log.i("MySdk", "tid 5000: " + util.tid)
                recursion(5000) {
                    Log.i("MySdk", "new threadSize: ${util.threadStackUsage}")
                }
            } catch (e: Error) {
                Log.i("MySdk", "5000")
            }
        }, "mytest5000").start()
        Thread({
            try {
                Log.i("MySdk", "tid 10000: " + util.tid)
                recursion(10000) {
                    Log.i("MySdk", "new threadSize: ${util.threadStackUsage}")
                }
            } catch (e: Error) {
                Log.i("MySdk", "10000")
            }
        }, "mytest10000").start()
        Thread({
            try {
                Log.i("MySdk", "tid 20000: " + util.tid)
                recursion(20000) {
                    Log.i("MySdk", "new threadSize: ${util.threadStackUsage}")
                }
            } catch (e: Error) {
                Log.i("MySdk", "20000")
            }
        }, "mytest20000").start()
        Log.i("MySdk", "new threadSize: ${util.threadStackUsage}")
    }


    private fun logStack(pre: String) {
        val memoryInfo: Debug.MemoryInfo = Debug.MemoryInfo()
        Debug.getMemoryInfo(memoryInfo)
        val stackMemoryKB = memoryInfo.totalPrivateDirty // 单位KB
        Log.i("MySdk", "xxxxx $pre threadSize: $stackMemoryKB")
    }


    private fun recursion(times: Int, invoke: () -> Unit) {
        if (times == 0) {
            invoke.invoke()
            return
        }
        recursion(times - 1, invoke)
    }

    fun printThreadStackInfo(threadId: Long, pre: String) {
        val pid: Int = android.os.Process.myPid()
        val path = "/proc/$pid/task/$threadId/smaps"
        try {
            BufferedReader(FileReader(path)).use { br ->
                val sb = StringBuilder()
                var count = 0
                br.readLines().forEach {
                    count++
                    sb.append(it).append("\n")
                    if (count == 10000000) {
                        return@forEach
                    }
                }
                Log.i("MySdk", "$pre printThreadStackInfo ${threadId} zi threadSize: $sb")
            }
        } catch (e: Exception) {
            // e.printStackTrace()
        }
    }

}