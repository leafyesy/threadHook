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
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <iostream>
#include <sstream>
#include <fstream>

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

#define LOGEP(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

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



extern "C"
JNIEXPORT jlong JNICALL
Java_com_ysydhc_threadhook_StackSizeUtil_getThreadStackSize(JNIEnv *env, jobject thiz, jlong thread_id) {
    // pthread_t thread = (pthread_t) thread_id;
    pthread_t thread = pthread_self();
    pthread_attr_t attr;
    size_t stack_size;

    int result = pthread_getattr_np(thread, &attr);
    if (result != 0) {
        fprintf(stderr, "Error getting thread attributes: %s\n", strerror(result));
        return 1;
    }

    pthread_attr_getstacksize(&attr, &stack_size);
    pthread_attr_destroy(&attr);

    return (jlong) stack_size;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_ysydhc_threadhook_StackSizeUtil_getStackSize(JNIEnv *env,jobject /* this */) {
  struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        printf("Max RSS: %ld KB\n", usage.ru_maxrss);
        return usage.ru_maxrss;
    } else {
        perror("getrusage");
    }
    return 0;
}


jlong get_thread_stack_usage(pid_t tid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/self/task/%d/stat", tid);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    // 读取 stat 文件中的内容
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), file) != NULL) {
        // 解析 stat 文件中的内容
        // 第28个字段是 startstack，表示栈的起始地址
        // 第29个字段是 kstkesp，表示当前栈指针
        unsigned long startstack, kstkesp;
        sscanf(buffer, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %*u %*u %*u %*u %lu %lu", &startstack, &kstkesp);

        printf("xxx Thread ID: %d\n", tid);
        printf("xxx Start stack: 0x%lx\n", startstack);
        printf("xxx Current stack pointer: 0x%lx\n", kstkesp);
        printf("xxx Stack usage: %lu bytes\n", startstack - kstkesp);
        fclose(file);
        return startstack - kstkesp;
    } else {
        fclose(file);
    }
    return 0;
}


void print_thread_stat(pid_t pid,pid_t tid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/task/%d/stat", pid, tid);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror("fopen");
        return;
    }

    // 读取 stat 文件中的内容
    char buffer[4096];
    if (fgets(buffer, sizeof(buffer), file) != NULL) {
        LOGEP("Thread stat for TID %d:\n%s\n", tid, buffer);
    } else {
        LOGEP("fgets");
    }

    fclose(file);
}


jlong get_stack_usage(pid_t pid, pid_t tid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/task/%d/stat", pid, tid);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    // 读取 stat 文件中的内容
    char buffer[4096];
    if (fgets(buffer, sizeof(buffer), file) != NULL) {
        // 解析 stat 文件中的内容
        // 第28个字段是 startstack，表示栈的起始地址
        // 第29个字段是 kstkesp，表示当前栈指针
        unsigned long startstack, kstkesp;

        int ret = sscanf(buffer, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %*u %*u %*u %*u %lu %lu", &startstack, &kstkesp);
        LOGEP("pid and thread id: %d %d start:%lu %lu",pid, tid, startstack, kstkesp);
        if (ret == 2) {
            printf("Start stack: 0x%lx\n", startstack);
            printf("Current stack pointer: 0x%lx\n", kstkesp);
            printf("Stack usage: %lu bytes\n", startstack - kstkesp);
        } else {
            fprintf(stderr, "Failed to parse /proc/self/task/%d/stat\n", tid);
        }
        fclose(file);
        print_thread_stat(pid,tid);
        return startstack - kstkesp;
    }

    fclose(file);
    return 0;
}

int getProcessId() {
    return getpid(); // 获取当前进程 ID
}

jlong printThreadStackInfo(long threadId) {
    int pid = getProcessId();
    std::stringstream path;
    path << "/proc/" << pid << "/task/" << threadId << "/status";

    std::ifstream file(path.str());
    if (!file.is_open()) {

        return -1;
    }
    long stackSizeKB;
    std::string line;
    while (getline(file, line)) {
        if (line.find("VmStk:") == 0) { // 检查是否是 Stack Size 行
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string stackSizePart = line.substr(colonPos + 1);
                std::stringstream stackStream(stackSizePart);
                stackStream >> stackSizeKB;
            }
        }
    }
    file.close();
    return stackSizeKB;
}


extern "C"
JNIEXPORT jlong JNICALL
Java_com_ysydhc_threadhook_StackSizeUtil_getThreadStackUsage(JNIEnv *env, jobject thiz) {
    pid_t pid = getpid();
    pid_t tid = syscall(SYS_gettid); // 获取当前线程的线程ID
    // return get_stack_usage(pid,tid);
    // return get_thread_stack_usage(tid);
    return printThreadStackInfo(tid);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_ysydhc_threadhook_StackSizeUtil_getTid(JNIEnv *env, jobject thiz) {
  pid_t tid = syscall(SYS_gettid);
  return tid;
}

