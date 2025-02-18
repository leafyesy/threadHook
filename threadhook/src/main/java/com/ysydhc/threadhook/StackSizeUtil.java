package com.ysydhc.threadhook;

public class StackSizeUtil {

    public native long getThreadStackSize(long threadID);

    public native long getStackSize();

    public native long getThreadStackUsage();

    public native long getTid();

}
