#include <jni.h>
#include <string>
#include <shadowhook.h>
#include <android/log.h>
#include <pthread.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include "thread_hook.h"
#include "thread_compressor.h"
#include <iostream>

#define LOG_TAG "thread_hook"
#define TARGET_ART_LIB "libart.so"
#define THREAD_NAME_SIZE 16

// readelf -Ws libart.so | grep CreateNativeThread
#if defined(__arm__) // ARMv7 32-bit
#define TARGET_CREATE_NATIVE_THREAD "_ZN3art6Thread18CreateNativeThreadEP7_JNIEnvP8_jobjectjb"
#elif defined(__aarch64__) // ARMv8 64-bit
#define TARGET_CREATE_NATIVE_THREAD "_ZN3art6Thread18CreateNativeThreadEP7_JNIEnvP8_jobjectmb"
#endif

#define SIZE_1M_BYTE (1 * 1024 * 1024)
#define SIZE_1KB_BYTE (1 * 1024)

namespace thread_hook {
    jobject callbackObj = nullptr;

    void *originalFunction = nullptr;
    void *stubFunction = nullptr;

    typedef void (*ThreadCreateFunc)(JNIEnv *env, jobject java_peer, size_t stack_size,
                                     bool is_daemon);

    bool currentThreadName(char *name) {
        return prctl(PR_GET_NAME, (unsigned long) name, 0, 0, 0) == 0;
    }

    void createNativeThreadProxy(JNIEnv *env, jobject java_peer, size_t stack_size, bool is_daemon) {
        char threadName[THREAD_NAME_SIZE];
        if (stack_size == 0) {
            long adjustment_size = thread_compressor::get_thread_stack_size();
            long default_size = SIZE_1M_BYTE;
            long final_size = default_size - adjustment_size;
            __android_log_print(ANDROID_LOG_INFO,
                                LOG_TAG,
                                "Adjusting thread size, target adjustment: %ld, thread name %s",
                                -final_size,
                                currentThreadName(threadName) ? threadName : "N/A");
            ((ThreadCreateFunc) originalFunction)(env, java_peer, -final_size, is_daemon);
        } else {
            ((ThreadCreateFunc) originalFunction)(env, java_peer, stack_size, is_daemon);
        }
    }

    void setNativeThreadStackFailed(JNIEnv *pEnv, const char *errMsg) {
        jclass jThreadHookClass = pEnv->FindClass("com/ysydhc/threadhook/ThreadSizeCallback");
        if (jThreadHookClass == nullptr) {
            return;
        }
        jmethodID jMethodId = pEnv->GetMethodID(jThreadHookClass, "setNativeThreadStackFailed",
                                                "(Ljava/lang/String;)V");
        if (jMethodId != nullptr) {
            pEnv->CallVoidMethod(callbackObj, jMethodId, pEnv->NewStringUTF(errMsg));
        }
    }

    void hook_create_native_thread(JNIEnv *pEnv) {
#if defined(__arm__) || defined(__aarch64__)
        stubFunction = shadowhook_hook_sym_name(TARGET_ART_LIB, TARGET_CREATE_NATIVE_THREAD,
                                                (void *) createNativeThreadProxy,
                                                (void **) &originalFunction);
        if (stubFunction == nullptr) {
            const int err_num = shadowhook_get_errno();
            const char *errMsg = shadowhook_to_errmsg(err_num);
            if (errMsg == nullptr || callbackObj == nullptr) {
                return;
            }
            setNativeThreadStackFailed(pEnv, errMsg);
            delete errMsg;
        }
        // this hook exist in the APP's whole lifecycle, so we do not need to release 'stubFunction' pointer.
#else
        setNativeThreadStackFailed(pEnv, "Unsupported architecture.");
#endif
    }
} // namespace thread_hook


extern "C" JNIEXPORT void JNICALL
Java_com_ysydhc_threadhook_ThreadHook_setNativeThreadStackSize(JNIEnv *env,
                                                                      jobject /* this */,
                                                                      jlong stackSizeKb,
                                                                      jobject callback) {
    long const target_size = stackSizeKb * SIZE_1KB_BYTE;

    thread_compressor::thread_stack_size(target_size);
    if (target_size > 0 && target_size < SIZE_1M_BYTE) {
        thread_hook::callbackObj = env->NewGlobalRef(callback);
        thread_hook::hook_create_native_thread(env);
    }
}