#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
// #define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

static inline bool is_thread_complete_success(struct thread_data *args)
{
    return args->thread_complete_success;
}

void* threadfunc(void* thread_param)
{
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    /* lock mutex */
    usleep(thread_func_args->wait_to_obtain_ms_*1000U);
    int ret;
    ret = pthread_mutex_lock(thread_func_args->mutex_);
    if(ret != 0) {
        ERROR_LOG("*** Can not lock mutex ***");
    }

    /* unlock mutex */
    usleep(thread_func_args->wait_to_release_ms_*1000U);
    ret = pthread_mutex_unlock(thread_func_args->mutex_);
    if(ret != 0) {
        ERROR_LOG("*** Can not unlock mutex ***");
    }

    thread_func_args->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data *thread_param = malloc(sizeof(struct thread_data));
    memset(thread_param, 0, sizeof(struct thread_data));

    thread_param->mutex_ = mutex;
    thread_param->wait_to_obtain_ms_ = wait_to_obtain_ms;
    thread_param->wait_to_release_ms_ = wait_to_release_ms;
    thread_param->thread_complete_success = false;

    int ret;
    ret = pthread_create(thread, NULL, threadfunc, (void *)thread_param);
    if(ret != 0) {
        ERROR_LOG("Fail to create pthreat");
        return false;
    }

    return true;
}

