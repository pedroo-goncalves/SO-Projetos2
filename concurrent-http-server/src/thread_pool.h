#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include "shared_mem.h"
#include "semaphores.h"

// Nó da Fila Local (Linked List)
typedef struct job_node {
    int client_fd;
    struct job_node* next;
} job_node_t;

typedef struct {
    pthread_t* threads;
    int num_threads;
    
    // Feature 2: Sincronização da Pool
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    
    // Feature 2: Fila de Tarefas Local
    job_node_t* head;
    job_node_t* tail;
    int count;
    
    int shutdown;
    void* args; 
} thread_pool_t;

// Argumentos passados às threads (agora mais simples)
typedef struct {
    thread_pool_t* pool;      
    shared_data_t* shared_data; 
    semaphores_t* sems;       
    // IPC socket removido daqui, pois quem lê é o Dispatcher (worker_main)
} thread_worker_args_t;

thread_pool_t* create_thread_pool(int num_threads, shared_data_t* shm, semaphores_t* sems);
void destroy_thread_pool(thread_pool_t* pool);

// Função nova para o Dispatcher enviar trabalho
void thread_pool_submit(thread_pool_t* pool, int client_fd);

#endif