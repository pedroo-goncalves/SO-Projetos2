#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include "shared_mem.h"
#include "semaphores.h"

// Estrutura para a Fila Local de Pedidos
typedef struct job_node {
    int client_fd;
    struct job_node* next;
} job_node_t;

typedef struct {
    pthread_t* threads;
    int num_threads;
    
    // Sincronização Interna (Feature 2)
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    
    // Fila Local (Protected by mutex)
    job_node_t* head;
    job_node_t* tail;
    int count;
    
    int shutdown;
    void* args; // Argumentos partilhados (shm, sems)
} thread_pool_t;

typedef struct {
    thread_pool_t* pool;      
    shared_data_t* shared_data; 
    semaphores_t* sems;       
    void* local_cache;
    int ipc_socket; 
} thread_worker_args_t;

// Cria a pool
thread_pool_t* create_thread_pool(int num_threads, shared_data_t* shm, semaphores_t* sems, int ipc_socket);

// Adiciona trabalho à fila local (Dispatcher chama isto)
void thread_pool_submit(thread_pool_t* pool, int client_fd);

// Limpeza completa (Join threads)
void destroy_thread_pool(thread_pool_t* pool);

#endif