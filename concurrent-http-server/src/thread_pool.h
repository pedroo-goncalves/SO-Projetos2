// thread_pool.h
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include "shared_mem.h" // <--- CRÍTICO: Define shared_data_t
#include "semaphores.h" // <--- CRÍTICO: Define semaphores_t

typedef struct {
    pthread_t* threads;
    int num_threads;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int shutdown;
    // Guardamos os argumentos aqui para poder libertar depois (opcional, mas boa prática)
    void* args; 
} thread_pool_t;

typedef struct {
    thread_pool_t* pool;      
    shared_data_t* shared_data; 
    semaphores_t* sems;       
    void* local_cache;
    int listen_fd; // <--- NOVO
} thread_worker_args_t;

thread_pool_t* create_thread_pool(int num_threads, shared_data_t* shm, semaphores_t* sems, int listen_fd);

void destroy_thread_pool(thread_pool_t* pool);

#endif