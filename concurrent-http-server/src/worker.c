#include "worker.h"
#include "thread_pool.h"
#include <unistd.h>
#include <stdio.h>

extern volatile int keep_running;

// Assinatura atualizada para receber o listen_fd
void worker_main(shared_data_t* shm, semaphores_t* sems, int listen_fd) {
    int num_threads = 10; 
    
    // Criar a pool passando o socket de escuta
    thread_pool_t* pool = create_thread_pool(num_threads, shm, sems, listen_fd);
    (void)pool;

    while (keep_running) {
        sleep(1);
    }
}