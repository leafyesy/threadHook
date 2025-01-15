package com.ysydhc.threadhook;

public class ThreadSizeCallback {

    void setNativeThreadStackFailed(String reason) {
        System.out.println("reason:" + reason);
    }

}
