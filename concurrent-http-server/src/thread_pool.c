// thread_pool.c
#include "thread_pool.h"
#include "semaphores.h"
#include "shared_mem.h"
#include "thread_pool.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

void* worker_thread(void* arg) {
    
    thread_worker_args_t* args = (thread_worker_args_t*)arg;
    shared_data_t* data = args->shared_data; // A Fila e Estatísticas (Memória Partilhada)
    semaphores_t* sems = args->sems;         // Os Semáforos (IPC)
    thread_pool_t* pool = args->pool;        // O estado do Pool (para shutdown)
    int client_fd;

    while (!pool->shutdown) {

        // espera por uma conexão (bloqueio)
        if (sem_wait(sems->filled_slots) < 0) {
             
            if (pool->shutdown) break; 
            if (errno == EINTR) break;

            continue; 
        }

        // BLOQUEIO MÚTUO: Obtemos o acesso exclusivo à estrutura de dados da fila na memória partilhada.
        // Isto impede que outros Workers ou o Master modifiquem simultaneamente os índices da fila.
        sem_wait(sems->queue_mutex);
        
        // --- ZONA CRÍTICA (CONSUMO) ---
        // retiramos o file descriptor da frente da fila partilhada.
        client_fd = data->queue.sockets[data->queue.front];
        data->queue.front = (data->queue.front + 1) % MAX_QUEUE_SIZE; // mover o ponteiro de forma circular
        data->queue.count--;

        // diminuimos o número de conexões ativas
        data->stats.active_connections--;
        // ------------------------------

        // LIBERTAÇÃO MÚTUA: O Worker termina de manipular a fila e liberta o acesso.
        sem_post(sems->queue_mutex);
        
        // SINALIZAÇÃO AO PRODUTOR: Notificamos o Master que um slot foi libertado.
        // O contador de 'empty_slots' aumenta, para que o Master produza mais uma conexão se estiver à espera.
        sem_post(sems->empty_slots);
        
        // fechamos o socket do cliente após o processamento estar completo.
        close(client_fd);
    }
    
    return NULL;

}

thread_pool_t* create_thread_pool(int num_threads) {

    thread_pool_t* pool = malloc(sizeof(thread_pool_t));

    pool->threads = malloc(sizeof(pthread_t) * num_threads);

    pool->num_threads = num_threads;

    pool->shutdown = 0;

    pthread_mutex_init(&pool->mutex, NULL);
    
    pthread_cond_init(&pool->cond, NULL);

    for (int i = 0; i < num_threads; i++) {

        pthread_create(&pool->threads[i], NULL, worker_thread, pool);

    }

    return pool;
}