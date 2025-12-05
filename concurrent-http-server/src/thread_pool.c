// src/thread_pool.c (Versão Pre-Fork Accept)
#include "thread_pool.h"
#include "semaphores.h"
#include "shared_mem.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h> // accept
#include <netinet/in.h>

// ... (função get_mime_type mantém-se igual) ...
const char* get_mime_type(const char* filename) {
    if (strstr(filename, ".html")) return "text/html";
    if (strstr(filename, ".css")) return "text/css";
    if (strstr(filename, ".js")) return "application/javascript";
    return "text/plain";
}

void* worker_thread(void* arg) {
    thread_worker_args_t* args = (thread_worker_args_t*)arg;
    shared_data_t* data = args->shared_data;
    semaphores_t* sems = args->sems;
    thread_pool_t* pool = args->pool;
    
    // O socket do servidor vem nos args (guardado no create)
    int server_socket = args->listen_fd; 
    int client_fd;
    char buffer[4096];

    while (!pool->shutdown) {
        
        // --- 1. ACCEPT (Protegido por Mutex) ---
        // Usamos o queue_mutex para garantir que apenas 1 thread de cada vez tenta aceitar
        // (Evita "Thundering Herd" em sistemas antigos, seguro em Linux moderno)
        sem_wait(sems->queue_mutex);
        
        client_fd = accept(server_socket, NULL, NULL);
        
        sem_post(sems->queue_mutex);

        if (client_fd < 0) {
            // Se falhar o accept (ex: outro worker pegou), tenta de novo
            continue; 
        }

        // Atualizar estatísticas
        sem_wait(sems->stats_mutex);
        data->stats.active_connections++;
        sem_post(sems->stats_mutex);

        // --- 2. PROCESSAR PEDIDO (Igual ao anterior) ---
        // Agora o FD é válido porque foi este processo que o criou!
        memset(buffer, 0, sizeof(buffer));
        if (read(client_fd, buffer, sizeof(buffer) - 1) > 0) {
            http_request_t req;
            if (parse_http_request(buffer, &req) == 0) {
                
                // LÓGICA DE NEGÓCIO (Copia do anterior)
                if (strstr(req.path, "/api/register")) {
                    char plan = ' ';
                    if (strstr(req.path, "plan=A")) plan = 'A';
                    if (strstr(req.path, "plan=B")) plan = 'B';
                    if (strstr(req.path, "plan=C")) plan = 'C';
                    
                    int success = 0;
                    char* msg = "Esgotado";
                    sem_wait(sems->stats_mutex);
                    if (plan=='A' && data->slots_A>0) { data->slots_A--; success=1; msg="Sucesso!"; }
                    else if (plan=='B' && data->slots_B>0) { data->slots_B--; success=1; msg="Sucesso!"; }
                    else if (plan=='C' && data->slots_C>0) { data->slots_C--; success=1; msg="Sucesso!"; }
                    sem_post(sems->stats_mutex);
                    
                    char json[128];
                    snprintf(json, sizeof(json), "{\"message\":\"%s\",\"success\":%s}", msg, success?"true":"false");
                    send_http_response(client_fd, success?200:409, "OK", "application/json", json, strlen(json));
                }
                else if (strstr(req.path, "/api/slots")) {
                    char json[128];
                    sem_wait(sems->stats_mutex);
                    snprintf(json, sizeof(json), "{\"A\":%d,\"B\":%d,\"C\":%d}", data->slots_A, data->slots_B, data->slots_C);
                    sem_post(sems->stats_mutex);
                    send_http_response(client_fd, 200, "OK", "application/json", json, strlen(json));
                }
                else {
                    char filepath[1024];
                    if (strcmp(req.path, "/") == 0) snprintf(filepath, sizeof(filepath), "www/index.html");
                    else snprintf(filepath, sizeof(filepath), "www%s", req.path);
                    
                    FILE* f = fopen(filepath, "rb");
                    if (f) {
                        fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
                        char* b=malloc(sz); fread(b,1,sz,f); fclose(f);
                        send_http_response(client_fd, 200, "OK", get_mime_type(filepath), b, sz);
                        free(b);
                    } else {
                        send_http_response(client_fd, 404, "Not Found", "text/plain", "404", 3);
                    }
                }
            }
        }
        close(client_fd);
        
        sem_wait(sems->stats_mutex);
        data->stats.active_connections--;
        sem_post(sems->stats_mutex);
    }
    return NULL;
}

// ATENÇÃO: Atualizar create_thread_pool para receber e guardar o listen_fd
thread_pool_t* create_thread_pool(int num_threads, shared_data_t* shm, semaphores_t* sems, int listen_fd) {
    thread_pool_t* pool = malloc(sizeof(thread_pool_t));
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    pool->num_threads = num_threads;
    pool->shutdown = 0;
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);

    thread_worker_args_t* args = malloc(sizeof(thread_worker_args_t));
    args->pool = pool;
    args->shared_data = shm;
    args->sems = sems;
    args->local_cache = NULL;
    
    // NOVO CAMPO: Precisas de adicionar 'int listen_fd' à struct thread_worker_args_t no header
    args->listen_fd = listen_fd; 

    pool->args = args; 

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread, args);
    }
    return pool;
}
// destroy_thread_pool mantém-se igual...
void destroy_thread_pool(thread_pool_t* pool) {
     if (!pool) return;
    if (pool->args) free(pool->args);
    if (pool->threads) free(pool->threads);
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    free(pool);
}