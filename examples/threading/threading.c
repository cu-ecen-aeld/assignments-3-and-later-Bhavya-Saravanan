#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>    
#include <errno.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)



void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;

   struct thread_data *args = (struct thread_data *)thread_param;
    if (args == NULL) {
        ERROR_LOG("threadfunc: got NULL thread_param\n");
        return thread_param;                 
    }

    // Assuming failure unless every step succeeds
    args->thread_complete_success = false;

    // wait BEFORE trying to lock (milliseconds → microseconds)
    usleep((useconds_t)args->wait_to_obtain_ms * 1000);

    // lock the mutex
    int rc = pthread_mutex_lock(args->mutex);
    if (rc != 0) {
        ERROR_LOG("threadfunc: mutex lock failed: %s\n", strerror(rc));
        return args;
    }
    DEBUG_LOG("threadfunc: mutex locked\n");

    // wait WHILE holding the lock
    usleep((useconds_t)args->wait_to_release_ms * 1000);

    // unlock the mutex
    rc = pthread_mutex_unlock(args->mutex);
    if (rc != 0) {
        ERROR_LOG("threadfunc: mutex unlock failed: %s\n", strerror(rc));
        return args;
    }
    printf("threadfunc: mutex unlocked\n");

    // All steps worked
    args->thread_complete_success = true;
    printf("threadfunc: success\n");
    return args;                              

}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    if (!thread || !mutex) return false;

    // allocate the argument block for the new thread
    struct thread_data *td = (struct thread_data *)malloc(sizeof *td);
    if (!td) return false;

    td->mutex = mutex;
    td->wait_to_obtain_ms  = wait_to_obtain_ms;
    td->wait_to_release_ms = wait_to_release_ms;
    td->thread_complete_success = false;

    // create the thread 
    int rc = pthread_create(thread, NULL, threadfunc, td);

    if (rc != 0) {
        ERROR_LOG("ERROR: Unable to create a new thread!");
        free(td);
        return false;
    }

    DEBUG_LOG("pthread create is successful");
    return true;
   
}

