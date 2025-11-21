// master.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h> 
#include <errno.h>

#include "master.h"
#include "http.h"


volatile sig_atomic_t keep_running = 1;

void signal_handler(int signum) {
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

    // procura um slot vazio, sem que bloqueie
    if (sem_trywait(sems->empty_slots) == -1) {

        if (errno == EAGAIN) {

            const char *msg = "503 Serviço Indisponível.";
            send_http_response(client_fd, 503, msg, "text/html", msg , strlen(msg));
            close(client_fd);
            
            return;

        }
        else {

            perror("Erro no produtor.");
            close(client_fd);

            return;

        }

    }

    // chegar aqui implica que tenha sido consumido um slot vazio 
    sem_wait(sems->queue_mutex);
    
    data->queue.sockets[data->queue.rear] = client_fd;
    data->queue.rear = (data->queue.rear + 1) % MAX_QUEUE_SIZE;
    data->queue.count++;

    // incrementa o número de conexões ativas em 1
    // está em memória partilhada, então deve ser protegida pelo queue_mutex
    data->stats.active_connections++;
    
    // libertar o mutex e sinalizar o slot preenchido
    sem_post(sems->queue_mutex);
    sem_post(sems->filled_slots);
    
}