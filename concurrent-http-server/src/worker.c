#include "worker.h"
#include "thread_pool.h"
#include "ipc.h"  // Necessário para recv_fd
#include <unistd.h>
#include <stdio.h>

extern volatile int keep_running;

void worker_main(shared_data_t* shm, semaphores_t* sems, int ipc_socket) {
    int num_threads = 10;
    thread_pool_t* pool = create_thread_pool(num_threads, shm, sems, ipc_socket);
    
    printf("[Worker] Dispatcher iniciado. A aguardar tarefas IPC...\n");

    while (keep_running) {
        // 1. Espera sinal do Mestre (IPC)
        if (sem_wait(sems->filled_slots) < 0) {
            if (!keep_running) break;
            continue;
        }

        // 2. Recebe FD do canal partilhado
        // Protegido por queue_mutex para garantir integridade do recv_fd
        sem_wait(sems->queue_mutex);
        int client_fd = recv_fd(ipc_socket);
        shm->queue.count--; // Atualizar contadores
        sem_post(sems->queue_mutex);
        sem_post(sems->empty_slots); // Avisar Mestre

        if (client_fd < 0) continue;

        // 3. Envia para a Thread Pool (Fila Local + CondVar)
        thread_pool_submit(pool, client_fd);
    }

    destroy_thread_pool(pool);
}