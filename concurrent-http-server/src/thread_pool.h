// thread_pool.h
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

typedef struct {

    pthread_t* threads;
    int num_threads;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int shutdown;
    
} thread_pool_t;

typedef struct {

    thread_pool_t* pool;      // Ponteiro para a próprio pool 
    shared_data_t* shared_data; // Ponteiro para a Memória Partilhada 
    semaphores_t* sems;       // Ponteiro para os Semáforos 
    void* local_cache;        // Ponteiro para a Cache local do processo Worker 

} thread_worker_args_t;

thread_pool_t* create_thread_pool(int num_threads);

void destroy_thread_pool(thread_pool_t* pool);

#endif