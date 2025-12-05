#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h> // Necessário para a thread de stats

#include "master.h"
#include "worker.h"
#include "shared_mem.h"
#include "semaphores.h"
#include "config.h"
#include "stats.h"   // Feature 3: Estatísticas

// Estrutura para passar argumentos à thread de monitorização
typedef struct {
    shared_data_t* data;
    semaphores_t* sems;
} monitor_args_t;

int main() {
    // 1. LIMPEZA PREVENTIVA
    // Limpa semáforos e memória antiga para evitar bloqueios
    sem_unlink("/ws_empty_v4");
    sem_unlink("/ws_filled_v4");
    sem_unlink("/ws_queue_mutex_v4");
    sem_unlink("/ws_stats_mutex_v4");
    sem_unlink("/ws_log_mutex_v4");
    shm_unlink("/webserver_shm");

    // Registar handler para Ctrl+C (Shutdown gracioso)
    signal(SIGINT, signal_handler);

    // 2. CONFIGURAÇÃO
    int port = 9090;       // Porta segura (evita conflitos com 8080)
    int num_workers = 4;   // Número de processos
    int queue_size = 100;  // Tamanho da fila (controlado por semáforos)

    printf("[INIT] A iniciar servidor...\n");

    // 3. MEMÓRIA PARTILHADA
    printf("[INIT] Criar Memoria Partilhada...\n");
    shared_data_t* shm = create_shared_memory();
    if (!shm) {
        perror("Falha ao criar SHM");
        return 1;
    }

    // Inicialização da Feature 3 (Stats)
    init_stats(shm);
    shm->stats.active_connections = 0;
    
    // Inicialização das Vagas (Ginásio)
    shm->slots_A = 20; 
    shm->slots_B = 15; 
    shm->slots_C = 5;

    // 4. SEMÁFOROS
    printf("[INIT] Inicializar Semaforos (v4)...\n");
    semaphores_t sems;
    if (init_semaphores(&sems, queue_size) < 0) {
        perror("Falha ao criar semaforos");
        return 1;
    }

    // 5. CANAL IPC (Feature 1 - Strict Producer-Consumer)
    // Criamos um par de sockets conectados:
    // sv[0] será usado pelo Mestre para ENVIAR
    // sv[1] será usado pelos Workers para RECEBER (partilhado)
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("Erro socketpair");
        return 1;
    }

    // 6. LANÇAR WORKERS (Processos Filhos)
    printf("[INIT] A lancar %d Workers...\n", num_workers);
    pid_t pids[num_workers];

    for (int i = 0; i < num_workers; i++) {
        if ((pids[i] = fork()) == 0) {
            // --- CÓDIGO DO WORKER ---
            close(sv[0]); // Fecha a ponta do Mestre
            
            // O Worker entra no seu loop principal
            // Passamos 'sv[1]' como o canal de leitura IPC
            worker_main(shm, &sems, sv[1]);
            
            exit(0); // Nunca deve chegar aqui
        }
    }

    // 7. CONFIGURAÇÃO DO MESTRE
    close(sv[1]); // Fecha a ponta dos Workers no processo pai

    // Criar o Socket TCP que escuta a internet
    int server_socket = create_server_socket(port);
    if (server_socket < 0) {
        perror("[ERRO FATAL] Falha ao criar server socket");
        // Matar workers se falhar
        for(int i=0; i<num_workers; i++) kill(pids[i], SIGKILL);
        return 1;
    }

    printf(">>> SERVIDOR ONLINE EM http://localhost:%d <<<\n", port);
    printf(">>> MODO: STRICT PRODUCER-CONSUMER (IPC) + THREAD POOL <<<\n");
    
    // 8. LANÇAR MONITOR DE ESTATÍSTICAS (Feature 3)
    pthread_t stats_tid;
    monitor_args_t stats_args = { .data = shm, .sems = &sems };
    
    if (pthread_create(&stats_tid, NULL, stats_monitor_thread, &stats_args) != 0) {
        perror("Aviso: Falha ao criar thread de estatisticas");
    }

    // 9. LOOP PRINCIPAL DO MESTRE (Produtor)
    // Passamos 'sv[0]' como o canal de envio IPC
    master_accept_loop(server_socket, shm, &sems, sv[0]);

    // 10. SHUTDOWN (Limpeza final quando o loop termina)
    printf("\n[SHUTDOWN] A encerrar servidor...\n");
    
    // Matar workers
    for (int i = 0; i < num_workers; i++) kill(pids[i], SIGTERM);
    
    // Esperar que terminem
    for (int i = 0; i < num_workers; i++) wait(NULL);
    
    // Fechar recursos
    close(server_socket);
    close(sv[0]); // Fechar canal IPC
    destroy_semaphores(&sems);
    destroy_shared_memory(shm);
    
    printf("[FIM] Adeus!\n");
    return 0;
}