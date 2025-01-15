#include <pthread.h>
#include <string>

namespace thread_compressor {

 int AdjustStackSize(pthread_attr_t *attr);
 void currentThreadName();

int compress(pthread_attr_t const *attr);

void thread_stack_size(long size);

long get_thread_stack_size();

}