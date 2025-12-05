#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "master.h"
#include "worker.h"
#include "shared_mem.h"
#include "semaphores.h"
#include "config.h"

int main() {
    // 1. Limpeza
    sem_unlink("/ws_empty_v4");
    sem_unlink("/ws_filled_v4");
    sem_unlink("/ws_queue_mutex_v4");
    sem_unlink("/ws_stats_mutex_v4");
    sem_unlink("/ws_log_mutex_v4");
    shm_unlink("/webserver_shm");
    signal(SIGINT, signal_handler);

    // 2. Config
    int port = 9090;
    int num_workers = 4;
    int queue_size = 100;

    printf("[INIT] Memoria Partilhada...\n");
    shared_data_t* shm = create_shared_memory();
    if (!shm) return 1;
    shm->stats.active_connections = 0;
    shm->slots_A = 20; shm->slots_B = 15; shm->slots_C = 5;

    printf("[INIT] Semaforos (v4)...\n");
    semaphores_t sems;
    // Alterei nomes para v4 para garantir limpeza
    if (init_semaphores(&sems, queue_size) < 0) return 1;

    // 3. CANAL IPC GLOBAL (A Solução)
    // sv[0] = Mestre Escreve
    // sv[1] = Workers Leem (Partilhado)
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("Erro socketpair"); return 1;
    }

    printf("[INIT] A lancar %d Workers...\n", num_workers);
    pid_t pids[num_workers];

    for (int i = 0; i < num_workers; i++) {
        if ((pids[i] = fork()) == 0) {
            // WORKER
            close(sv[0]); // Fecha escrita
            worker_main(shm, &sems, sv[1]); // Passa leitura partilhada
            exit(0);
        }
    }

    // MESTRE
    close(sv[1]); // Fecha leitura
    int server_socket = create_server_socket(port);
    if (server_socket < 0) {
        for(int i=0; i<num_workers; i++) kill(pids[i], SIGKILL);
        return 1;
    }

    printf(">>> SERVIDOR ONLINE EM http://localhost:%d <<<\n", port);
    
    // Passamos sv[0] como o canal de envio
    master_accept_loop(server_socket, shm, &sems, sv[0]);

    // Shutdown
    printf("\n[SHUTDOWN]...\n");
    for (int i = 0; i < num_workers; i++) kill(pids[i], SIGTERM);
    for (int i = 0; i < num_workers; i++) wait(NULL);
    close(server_socket);
    destroy_semaphores(&sems);
    destroy_shared_memory(shm);
    return 0;
}