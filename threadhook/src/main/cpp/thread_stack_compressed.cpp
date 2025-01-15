#include "jni.h"
#include <string>
#include <android/log.h>
#include <pthread.h>
#include <cerrno>
#include <sys/prctl.h>

// Logging levels
#define LOG_TAG "thread_hook"
#define LOG_ERROR ANDROID_LOG_ERROR
#define LOG_DEBUG ANDROID_LOG_DEBUG

// Logging macros for convenience
#define LOGE(TAG, fmt, ...) __android_log_print(LOG_ERROR, TAG, fmt, ##__VA_ARGS__)
#define LOGD(TAG, fmt, ...) __android_log_print(LOG_DEBUG, TAG, fmt, ##__VA_ARGS__)

// Branch prediction hints
#ifndef LIKELY
#define LIKELY(cond) (__builtin_expect(!!(cond), 1))
#endif
#ifndef UNLIKELY
#define UNLIKELY(cond) (__builtin_expect(!!(cond), 0))
#endif

// Size constants for stack adjustments
#define SIZE_16K (16 * 1024)
#define SIZE_1M (1 * 1024 * 1024)

// Thread name size
#define THREAD_NAME_SIZE 16

// Error codes for clarity
#define ERROR_GET_STACK_SIZE_FAILED -1
#define ERROR_ADJUST_STACK_SIZE_FAILED -2
#define ERROR_IGNORE_COMPRESS -3
#define ERROR_INVALID_STACK_SIZE -4
#define ERROR_STACK_SIZE_TOO_SMALL -5

namespace thread_compressor {
    long stack_size = SIZE_1M;

    // Checks if the given size is within an acceptable range for stack size adjustment
    bool IsSizeValid(size_t originSize) {
        constexpr size_t STACK_SIZE_OFFSET = SIZE_16K;
        constexpr size_t ONE_MB = SIZE_1M;
        return ONE_MB - STACK_SIZE_OFFSET <= originSize && originSize <= ONE_MB + STACK_SIZE_OFFSET;
    }

    static void currentThreadName();

    // Adjusts the stack size for a thread, if possible and necessary
    static int AdjustStackSize(pthread_attr_t *attr) {
        size_t origStackSize = 0;
        int ret = pthread_attr_getstacksize(attr, &origStackSize);
        if (UNLIKELY(ret != 0)) {
            LOGE(LOG_TAG, "Fail to call pthread_attr_getstacksize, ret: %d", ret);
            return ERROR_GET_STACK_SIZE_FAILED;
        }

        if (!IsSizeValid(origStackSize)) {
            LOGE(LOG_TAG, "Origin Stack size %u, give up adjusting.", origStackSize);
            return ERROR_INVALID_STACK_SIZE;
        }

        if (origStackSize < 2 * PTHREAD_STACK_MIN) {
            LOGE(LOG_TAG, "Stack size is too small to reduce, give up adjusting.");
            return ERROR_STACK_SIZE_TOO_SMALL;
        }
        if (stack_size > origStackSize){
            stack_size = origStackSize >> 1U;
        }
        size_t final_stack_size = stack_size;
        ret = pthread_attr_setstacksize(attr, final_stack_size);
        if (LIKELY(ret == 0)) {
            LOGE(LOG_TAG, "min size is %d", final_stack_size);
            currentThreadName();
        } else {
            LOGE(LOG_TAG, "Fail to call pthread_attr_setstacksize, ret: %d", ret);
            return ERROR_ADJUST_STACK_SIZE_FAILED;
        }
        return ret;
    }

    static void currentThreadName() {
        char threadName[THREAD_NAME_SIZE];
        if (prctl(PR_GET_NAME, (unsigned long)threadName, 0, 0, 0) != 0) {
            LOGD(LOG_TAG, "Acquire current thread name failed.");
            return;
        }
        LOGD(LOG_TAG, "Shrink thread stack size successfully, thread name: %s", threadName);

    }

    // Public interface to attempt stack size compression on a thread
    int compress(pthread_attr_t const *attr) {
        if (attr == nullptr) {
            LOGD(LOG_TAG, "attr is null, skip adjusting.");
            return ERROR_IGNORE_COMPRESS; // Using a defined error code here might improve clarity
        }
        return AdjustStackSize(const_cast<pthread_attr_t *>(attr));
    }

    void thread_stack_size(long size) {
        stack_size = size;
    }

    long get_thread_stack_size() {
        return stack_size;
    }
}