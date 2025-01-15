package com.ysydhc.threadhook;

import android.util.Log;

public class StackPrinter {

    private static final String TAG = "StackPrinter";
    private static final String LOG_TAG = "thread_hook";


    public static void print() {
        Exception exception = new Exception();
        // exception.fillInStackTrace();
        StackTraceElement[] stackTrace = exception.getStackTrace();
        for (StackTraceElement element : stackTrace) {
            Log.i(LOG_TAG, element.getClassName() + "." + element.getMethodName() + "()");
        }
    }

}
