#include <pthread.h>

namespace thread_hook {

void proxy(pthread_t *pthread_ptr, pthread_attr_t const *attr, void *(*start_routine)(void *),void *args);

void bind_proxy();

void thread_hook();

void thread_unhook();

}