#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h> 
#include <errno.h>
#include <string.h>
#include "master.h"
#include "http.h"
#include "ipc.h"

volatile sig_atomic_t keep_running = 1;
void signal_handler(int signum) { (void)signum; keep_running = 0; }

int create_server_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
    if (listen(sockfd, 128) < 0) return -1;
    return sockfd;
}

void master_accept_loop(int listen_fd, shared_data_t* data, semaphores_t* sems, int ipc_socket) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd;

    printf("Master: A aceitar conexoes (Shared IPC Channel)...\n"); fflush(stdout);

    while (keep_running) {
        // 1. ACCEPT
        client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) { if (errno == EINTR) continue; perror("Accept"); continue; }

        printf("[Master] Nova conexao! FD: %d\n", client_fd); fflush(stdout);

        // 2. PRODUCER
        if (sem_trywait(sems->empty_slots) == -1) {
            printf("[Master] Fila Cheia\n");
            close(client_fd);
            continue;
        }

        sem_wait(sems->queue_mutex);
        data->queue.count++;
        sem_post(sems->queue_mutex);

        // 3. ENVIAR PARA O CANAL PARTILHADO
        // Qualquer worker que acorde vai ler deste socket único
        if (send_fd(ipc_socket, client_fd) < 0) {
            perror("[Master] Erro send_fd");
        } else {
            sem_post(sems->filled_slots); // Acorda um worker
            printf("[Master] FD enviado para pool.\n"); fflush(stdout);
        }
        
        close(client_fd);
    }
}