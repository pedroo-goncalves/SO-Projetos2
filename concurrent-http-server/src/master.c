// master.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h> 
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>

#include "master.h"
#include "http.h"


volatile sig_atomic_t keep_running = 1;

void signal_handler(int signum) {
    (void)signum; // Silencia o warning
    keep_running = 0;
}

int create_server_socket(int port) {

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) return -1;

    int opt = 1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;

    addr.sin_addr.s_addr = INADDR_ANY;

    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {

        close(sockfd);

        return -1;
    }

    if (listen(sockfd, 128) < 0) {

        close(sockfd);
        return -1;
    
    }

    return sockfd;

}

void enqueue_connection(shared_data_t* data, semaphores_t* sems, int client_fd) {
    // Debug 1
    printf("[Master] A tentar obter slot vazio...\n"); fflush(stdout);
    
    if (sem_trywait(sems->empty_slots) == -1) {
        if (errno == EAGAIN) {
            printf("[Master] FILA CHEIA! A rejeitar conexão %d\n", client_fd); fflush(stdout);
            const char *msg = "503 Serviço Indisponível.";
            send_http_response(client_fd, 503, msg, "text/html", msg , strlen(msg));
            close(client_fd);
            return;
        } else {
            perror("[Master] Erro no sem_trywait");
            close(client_fd);
            return;
        }
    }

    // Debug 2
    printf("[Master] Slot obtido. A aguardar mutex da fila...\n"); fflush(stdout);
    
    sem_wait(sems->queue_mutex);
    
    // Debug 3
    printf("[Master] Mutex obtido. A escrever na memória...\n"); fflush(stdout);
    
    data->queue.sockets[data->queue.rear] = client_fd;
    data->queue.rear = (data->queue.rear + 1) % MAX_QUEUE_SIZE;
    data->queue.count++;
    
    sem_wait(sems->stats_mutex);
    data->stats.active_connections++;
    sem_post(sems->stats_mutex);
    
    sem_post(sems->queue_mutex);
    sem_post(sems->filled_slots);
    
    printf("[Master] SUCESSO! Conexão %d entregue aos Workers.\n", client_fd); 
    fflush(stdout);
}

void master_accept_loop(int listen_fd, shared_data_t* data, semaphores_t* sems) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd;

    printf("Master: A aceitar conexões na porta...\n");
    fflush(stdout); // <--- FORÇA O TEXTO A APARECER

    while (keep_running) {
        // Bloqueia aqui à espera do browser
        client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);

        if (client_fd < 0) {
            if (errno == EINTR) continue; 
            perror("[Master] Erro no accept");
            continue;
        }

        printf("[Master] Nova conexão recebida! FD: %d\n", client_fd);
        fflush(stdout); // <--- FORÇA O TEXTO A APARECER
        
        enqueue_connection(data, sems, client_fd);
    }
}