#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "master.h"
#include "worker.h"
#include "shared_mem.h"
#include "semaphores.h"
#include "config.h"

int main() {
    // 1. LIMPEZA E SINAIS
    sem_unlink("/ws_empty_v2");
    sem_unlink("/ws_filled_v2");
    sem_unlink("/ws_queue_mutex_v2");
    sem_unlink("/ws_stats_mutex_v2");
    sem_unlink("/ws_log_mutex_v2");
    shm_unlink("/webserver_shm");
    signal(SIGINT, signal_handler);

    // 2. INICIALIZAÇÃO
    int port = 9090;
    int num_workers = 4;
    int queue_size = 100; // Agora usado para semáforos, não para sockets

    printf("[INIT] A iniciar sistema...\n");
    shared_data_t* shm = create_shared_memory();
    if (!shm) return 1;
    
    // Inicializar Estado
    shm->stats.active_connections = 0;
    shm->slots_A = 20; shm->slots_B = 15; shm->slots_C = 5;

    semaphores_t sems;
    if (init_semaphores(&sems, queue_size) < 0) return 1;

    // --- MUDANÇA CRÍTICA: SOCKET CRIADO ANTES DO FORK ---
    printf("[INIT] A abrir porta %d...\n", port);
    int server_socket = create_server_socket(port);
    if (server_socket < 0) {
        perror("Erro fatal no socket");
        return 1;
    }
    // ----------------------------------------------------

    // 3. CRIAR WORKERS (Passamos o server_socket para eles)
    printf("[INIT] A lançar %d Workers...\n", num_workers);
    pid_t pids[num_workers];
    for (int i = 0; i < num_workers; i++) {
        if ((pids[i] = fork()) == 0) {
            // Filho: Vai aceitar conexões diretamente
            worker_main(shm, &sems, server_socket);
            exit(0);
        }
    }

    printf(">>> SERVIDOR ONLINE EM http://localhost:%d <<<\n", port);
    
    // 4. MESTRE (Modo Supervisor)
    // O Mestre já não aceita conexões, apenas segura o processo.
    // Podes adicionar aqui prints de estatísticas se quiseres.
    while (keep_running) {
        sleep(1);
    }

    // 5. SHUTDOWN
    printf("\n[SHUTDOWN] A encerrar...\n");
    for (int i = 0; i < num_workers; i++) kill(pids[i], SIGTERM);
    for (int i = 0; i < num_workers; i++) wait(NULL);
    close(server_socket);
    destroy_semaphores(&sems);
    destroy_shared_memory(shm);
    
    return 0;
}