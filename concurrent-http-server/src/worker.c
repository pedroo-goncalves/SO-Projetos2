#include "worker.h"
#include "thread_pool.h"
#include "ipc.h"
#include "cache.h" // <--- NOVO
#include <unistd.h>
#include <stdio.h>

extern volatile int keep_running;

void worker_main(shared_data_t* shm, semaphores_t* sems, int ipc_socket) {
    int num_threads = 10;
    
    // 1. INICIALIZAR CACHE LOCAL (10 MB)
    cache_t* cache = cache_init(10 * 1024 * 1024);
    printf("[Worker] Cache LRU inicializada (10MB).\n");

    // 2. Passar cache para a pool
    // ATENÇÃO: Precisamos de atualizar create_thread_pool para aceitar a cache
    thread_pool_t* pool = create_thread_pool(num_threads, shm, sems, cache);
    
    printf("[Worker] Dispatcher iniciado. A aguardar tarefas IPC...\n");

    while (keep_running) {
        if (sem_wait(sems->filled_slots) < 0) {
            if (!keep_running) break;
            continue;
        }

        sem_wait(sems->queue_mutex);
        int client_fd = recv_fd(ipc_socket);
        shm->queue.count--; 
        sem_post(sems->queue_mutex);
        sem_post(sems->empty_slots);

        if (client_fd < 0) continue;

        thread_pool_submit(pool, client_fd);
    }

    destroy_thread_pool(pool);
    cache_destroy(cache); // Limpeza final
}