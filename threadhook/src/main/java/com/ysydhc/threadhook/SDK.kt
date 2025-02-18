package com.ysydhc.threadhook

import android.os.Debug
import android.util.Log
import com.bytedance.shadowhook.ShadowHook
import com.bytedance.shadowhook.ShadowHook.ConfigBuilder
import java.io.BufferedReader
import java.io.FileReader


object MySdk {

    private val util = StackSizeUtil()

    fun init() {
        ShadowHook.init(
            ConfigBuilder().setMode(ShadowHook.Mode.UNIQUE).build()
        )
        StackPrinter.print()
        val size = util.getThreadStackSize(Thread.currentThread().id)
        Log.i("MySdk", "threadSize: $size")
//        Thread {
//            val size = util.getThreadStackSize(Thread.currentThread().id)
//            Log.i("MySdk", "zi threadSize: $size")
//            recursion(10) {
//                Log.i("MySdk", "${Thread.currentThread().id} zi threadSize: ${util.threadStackUsage}")
//                Log.i("MySdk", "${Thread.currentThread().id} zi threadSize: ${printThreadStackInfo(util.tid)}")
//            }
//        }.start()
        Thread {
            recursion(100) {
                printThreadStackInfo(util.tid, "100")
            }
        }.start()
        Thread {
            recursion(1000) {
                printThreadStackInfo(util.tid, "1000")
            }
        }.start()
        Thread {
            try {
                recursion(5000) {
                    printThreadStackInfo(util.tid, "5000")
                }
            } catch (e: Error) {
                Log.i("MySdk", "5000")
            }
        }.start()
        Thread {
            try {
                recursion(10000) {
                    printThreadStackInfo(util.tid, "10000")
                }
            } catch (e: Error) {
                Log.i("MySdk", "10000")
            }
        }.start()
        Thread {
            try {
                recursion(20000) {
                    printThreadStackInfo(util.tid, "20000")
                }
            } catch (e: Error) {
                Log.i("MySdk", "20000")
            }
        }.start()
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
        val path = "/proc/$pid/task/$threadId/stat"
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
//                var line: String
//                while (br.readLine().also { line = it } != null) {
//                    if (line.startsWith("VmStk:")) {
//                        // 示例输出：VmStk:    1234 kB
//                        val parts =
//                            line.split("\\s+".toRegex()).dropLastWhile { it.isEmpty() }.toTypedArray()
//                        if (parts.size >= 3) {
//                            val stackSizeKB = parts[1].toLong()
//                            Log.i("MySdk", "${Thread.currentThread().id} zi threadSize: $stackSizeKB")
//                            println(
//                                "Thread " + threadId +
//                                        " Stack Size: " + stackSizeKB + " KB"
//                            )
//                        }
//                    }
//                }
            }
        } catch (e: Exception) {
            // e.printStackTrace()
        }
    }

}