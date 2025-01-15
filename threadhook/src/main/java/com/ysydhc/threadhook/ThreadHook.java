package com.ysydhc.threadhook;

public class ThreadHook {

    native void setNativeThreadStackSize(long stackSizeKb, ThreadSizeCallback callback);

    native void threadHook();

    native void setStackSize(int size);

    native void threadUnhook();
}
