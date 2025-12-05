#include "semaphores.h"
#include <fcntl.h>
#include <stdio.h>

int init_semaphores(semaphores_t* sems, int queue_size) {
    // MUDANÇA: Adicionei "_v2" para garantir que são novos
    sems->empty_slots = sem_open("/ws_empty_v2", O_CREAT, 0666, queue_size);
    sems->filled_slots = sem_open("/ws_filled_v2", O_CREAT, 0666, 0);
    sems->queue_mutex = sem_open("/ws_queue_mutex_v2", O_CREAT, 0666, 1);
    sems->stats_mutex = sem_open("/ws_stats_mutex_v2", O_CREAT, 0666, 1);
    sems->log_mutex = sem_open("/ws_log_mutex_v2", O_CREAT, 0666, 1);
    
    if (sems->empty_slots == SEM_FAILED || 
        sems->filled_slots == SEM_FAILED || 
        sems->queue_mutex == SEM_FAILED || 
        sems->stats_mutex == SEM_FAILED || 
        sems->log_mutex == SEM_FAILED) {
        perror("Erro ao abrir semáforos");
        return -1;
    }
    return 0;
}

void destroy_semaphores(semaphores_t* sems) {
    sem_close(sems->empty_slots);
    sem_close(sems->filled_slots);
    sem_close(sems->queue_mutex);
    sem_close(sems->stats_mutex);
    sem_close(sems->log_mutex);
    
    sem_unlink("/ws_empty_v2");
    sem_unlink("/ws_filled_v2");
    sem_unlink("/ws_queue_mutex_v2");
    sem_unlink("/ws_stats_mutex_v2");
    sem_unlink("/ws_log_mutex_v2");
}