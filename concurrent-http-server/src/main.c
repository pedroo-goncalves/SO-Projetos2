#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h> // Necessário para a thread de stats
#include <getopt.h>

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

// Definição das opções longas para getopt_long
static struct option long_options[] = {
    {"config", required_argument, 0, 'c'},
    {"port", required_argument, 0, 'p'},
    {"workers", required_argument, 0, 'w'},
    {"threads", required_argument, 0, 't'},
    {"daemon", no_argument, 0, 'd'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 0},
    {0, 0, 0, 0},
    
};

void usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s [OPTIONS]\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -c, --config PATH Configuration file path (default: ./server.conf)\n");
    fprintf(stderr, " -p, --port PORT   Port to listen on (default: 8080)\n");
    fprintf(stderr, " -w, --workers NUM Number of worker processes (default: 4)\n");
    fprintf(stderr, " -t, --threads NUM Threads per worker (default: 10)\n");
    fprintf(stderr, " -d, --daemon      Run in background\n");
    fprintf(stderr, " -v, --verbose     Enable verbose logging\n");
    fprintf(stderr, " -h, --help        Show this help message\n");
    fprintf(stderr, "     --version     Show version information\n");
}

int main(int argc, char *argv[]) {
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
    server_config_t config;

    strncpy(config.config_path, "./server.conf", sizeof(config.config_path));
    config.config_path[sizeof(config.config_path) - 1] = '\0';

    //Inicializar flags
    config.is_daemon = 0;
    config.is_verbose = 0;

    // Apenas a opção -c deve ser lida primeiro para garantir que o ficheiro correto seja carregado.
    // Fazemos um loop duplo para garantir que o ficheiro de config é processado antes dos outros.
    int c;
    int option_index = 0;

    optind = 1;
    while ((c = getopt_long(argc, argv, "+c:p:w:t:dvh", long_options, &option_index)) != -1) {
        if (c == 'c'){
            strncpy(config.config_path, optarg, sizeof(config.config_path) - 1);
            config.config_path[sizeof(config.config_path) - 1] = '\0';
        }
    }       

    if (load_config(config.config_path, &config) != 0) {
        fprintf(stderr, "Aviso: Não foi possível carregar o ficheiro '%s'.\n", config.config_path);
        return EXIT_FAILURE;
    }

    //Reset getopt
    optind = 1;

    while ((c = getopt_long(argc, argv, "c:p:w:t:dvh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'c':
                //Já foi feito
                break;
            case 'p':
                config.port = atoi(optarg);
                break;
            case 'w':
                config.num_workers = atoi(optarg);
                break;
            case 't':
                config.threads_per_worker = atoi(optarg);
                break;
            case 'd':
                config.is_daemon = 1;
                break;
            case 'v':
                config.is_verbose = 1;
                break;
            case 'h':
                usage(argv[0]);
                return EXIT_SUCCESS;
            case '?':
                //Opção desconhecida
                usage(argv[0]);
                return EXIT_FAILURE;
            case 0: //OPções longas sem short option
                if (strcmp("version", long_options[option_index].name) == 0) {
                    printf("ConcurrentHTTP Server Versão 1.0\n");
                    return EXIT_SUCCESS;
                }
                break;
        }
    }

    printf("Configurações carregadas com sucesso.\n");
    printf("PORT: %d\n", config.port);
    printf("DOCUMENT_ROOT: %s\n", config.document_root);
    printf("NUM_WORKERS: %d\n", config.num_workers);
    printf("THREADS_PER_WORKER: %d\n", config.threads_per_worker);
    printf("MAX_QUEUE_SIZE: %d\n", config.max_queue_size);
    printf("CACHE_SIZE_MB: %d\n", config.cache_size_mb);
    printf("LOG_FILE: %s\n", config.log_file);
    printf("TIMEOUT_SECONDS: %d\n", config.timeout_seconds);
    
    if (config.is_daemon) {
        printf("[INIT] A iniciar em modo daemon (background)...\n");
        //Implementar a lógica (fork, setsid, close FDs)
    }

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
    if (init_semaphores(&sems, config.max_queue_size) < 0) {
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
    printf("[INIT] A lancar %d Workers...\n", config.num_workers);
    pid_t pids[config.num_workers];

    for (int i = 0; i < config.num_workers; i++) {
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
    int server_socket = create_server_socket(config.port);
    if (server_socket < 0) {
        perror("[ERRO FATAL] Falha ao criar server socket");
        // Matar workers se falhar
        for(int i=0; i<config.num_workers; i++) kill(pids[i], SIGKILL);
        return 1;
    }

    printf(">>> SERVIDOR ONLINE EM http://localhost:%d <<<\n", config.port);
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
    for (int i = 0; i < config.num_workers; i++) kill(pids[i], SIGTERM);
    
    // Esperar que terminem
    for (int i = 0; i < config.num_workers; i++) wait(NULL);
    
    // Fechar recursos
    close(server_socket);
    close(sv[0]); // Fechar canal IPC
    destroy_semaphores(&sems);
    destroy_shared_memory(shm);
    
    printf("[FIM] Adeus!\n");
    return 0;
}