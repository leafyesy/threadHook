#include <jni.h>
#include <string>
#include <shadowhook.h>
#include <android/log.h>
#include <pthread.h>
#include "thread_hook.h"
#include "thread_compressor.h"
#include <iostream>

#define LOG_TAG "thread_hook"
#define TARGET_LIB "libc.so"
#define TARGET_FUNC "pthread_create"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

JavaVM* javaVM; // 全局保存 JavaVM 的引用

void printJavaStackTrace2(JNIEnv* env) {
  // 获取当前线程的类加载器
    jclass threadClass = env->FindClass("java/lang/Thread");
    jmethodID currentThreadMethod = env->GetStaticMethodID(threadClass, "currentThread", "()Ljava/lang/Thread;");
    jobject currentThread = env->CallStaticObjectMethod(threadClass, currentThreadMethod);
    jmethodID getContextClassLoaderMethod = env->GetMethodID(threadClass, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
    jobject classLoader = env->CallObjectMethod(currentThread, getContextClassLoaderMethod);

    // 获取 StackPrinter 类
    jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
    jmethodID loadClassMethod = env->GetMethodID(classLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring className = env->NewStringUTF("com.ysydhc.threadhook.StackPrinter");
    jclass stackPrinterClass = (jclass)env->CallObjectMethod(classLoader, loadClassMethod, className);
    env->DeleteLocalRef(className);

    if (stackPrinterClass == nullptr) {
        LOGE("Failed to find StackPrinter class");
        env->ExceptionClear(); // 清除异常
        return;
    }

    // 获取 print 方法 ID
    jmethodID printMethod = env->GetStaticMethodID(stackPrinterClass, "print", "()V");
    if (printMethod == nullptr) {
        LOGE("Failed to find print method");
        env->ExceptionClear(); // 清除异常
        env->DeleteLocalRef(stackPrinterClass);
        return;
    }

    // 调用 print 方法
    env->CallStaticVoidMethod(stackPrinterClass, printMethod);

    // 检查是否有新的异常发生
    if (env->ExceptionCheck()) {
        LOGE("An exception occurred while calling print");
        env->ExceptionClear(); // 清除异常
    }

    // 释放局部引用
    env->DeleteLocalRef(stackPrinterClass);
}

static void printJavaStackTrace(JNIEnv *env) {
    __android_log_print(ANDROID_LOG_INFO,
                            LOG_TAG,
                            "start print : %s" , env);
    // 获取 Throwable 类
    jclass throwableClass = env->FindClass("java/lang/Throwable");
    if (throwableClass == nullptr) {
          __android_log_print(ANDROID_LOG_INFO,LOG_TAG,"Failed to find Throwable class");
          return;
      }
      // 获取 Throwable 构造方法 ID
    jmethodID throwableConstructor = env->GetMethodID(throwableClass, "<init>", "(Ljava/lang/String;)V");
    if (throwableConstructor == nullptr) {
        __android_log_print(ANDROID_LOG_INFO,LOG_TAG,"Failed to find Throwable constructor");
        return;
    }
    // 创建 Throwable 对象
    jstring message = env->NewStringUTF("Capturing stack trace");
    jobject throwable = env->NewObject(throwableClass, throwableConstructor, message);
    env->DeleteLocalRef(message);
    if (throwable == nullptr) {
        __android_log_print(ANDROID_LOG_INFO,LOG_TAG,"Failed to create Throwable object");
        return;
    }
      __android_log_print(ANDROID_LOG_INFO,
                            LOG_TAG,
                            "enter checked");
      // 获取 toString 方法 ID
      jmethodID toStringMethod = env->GetMethodID(throwableClass, "toString", "()Ljava/lang/String;");
      if (toStringMethod == nullptr) {
          std::cerr << "Failed to find toString method" << std::endl;
          __android_log_print(ANDROID_LOG_INFO,
                            LOG_TAG,
                            "Failed to find toString method");
          return;
      }
      // 调用 printStackTrace 方法
      jstring stackTraceString = (jstring)env->CallObjectMethod(throwable, toStringMethod);
      const char* stackTraceChars = env->GetStringUTFChars(stackTraceString, nullptr);
      __android_log_print(ANDROID_LOG_INFO,
                            LOG_TAG,
                            "stack : %s", stackTraceChars);
      LOGE("thread_hook Stack trace: %s", stackTraceChars);
      // 释放局部引用
      env->ReleaseStringUTFChars(stackTraceString, stackTraceChars);
      env->DeleteLocalRef(stackTraceString);
      env->DeleteLocalRef(throwable);
      env->DeleteLocalRef(throwableClass);
}

extern "C" JNIEXPORT void JNICALL
Java_com_ysydhc_threadhook_ThreadHook_threadHook(JNIEnv *env,jobject /* this */) {
    thread_hook::thread_hook();
}

extern "C" JNIEXPORT void JNICALL
Java_com_ysydhc_threadhook_ThreadHook_setStackSize(JNIEnv *env,jobject /* this */, jint size) {
    thread_compressor::thread_stack_size(size);
}

extern "C" JNIEXPORT void JNICALL
Java_com_ysydhc_threadhook_ThreadHook_threadUnhook(JNIEnv *env,jobject /* this */, jint size) {
    thread_hook::thread_unhook();
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    javaVM = vm; // 保存 JavaVM 的引用
    return JNI_VERSION_1_6;
}

namespace thread_hook {
    void *orig = NULL;
    void *stub = NULL;

    typedef void (*type_t)(pthread_t *pthread_ptr, pthread_attr_t const *attr,void *(*start_routine)(void *), void *args);

    void proxy(pthread_t *pthread_ptr, pthread_attr_t const *attr, void *(*start_routine)(void *),void *args) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "proxy pthread_create called.");
        JNIEnv* env;
        javaVM->AttachCurrentThread(&env, nullptr);
        if(javaVM == nullptr) {
          __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "javaVM is nullptr.");
        }
        printJavaStackTrace2(env);
        thread_compressor::compress(attr);
        ((type_t) orig)(pthread_ptr, attr, start_routine, args);
    }

    void bind_proxy() {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "bind_proxy start.");

        stub = shadowhook_hook_sym_name(
                TARGET_LIB,
                TARGET_FUNC,
                (void *) proxy,
                (void **) &orig);

        if (stub == NULL) {
            int err_num = shadowhook_get_errno();
            const char *err_msg = shadowhook_to_errmsg(err_num);
        }
    }

    void thread_hook() {
        bind_proxy();
    }

    void thread_unhook() {
        shadowhook_unhook(stub);
        stub = NULL;
        orig = NULL;
    }
}