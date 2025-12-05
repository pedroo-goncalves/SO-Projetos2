#include "thread_pool.h"
#include "semaphores.h"
#include "shared_mem.h"
#include "http.h"
#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

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
    int client_fd;
    char buffer[4096];

    while (1) {
        // --- FEATURE 2: Esperar na Condition Variable ---
        pthread_mutex_lock(&pool->mutex);
        
        // Enquanto não houver trabalho e não for para sair, espera
        while (pool->head == NULL && !pool->shutdown) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }

        if (pool->shutdown && pool->head == NULL) {
            pthread_mutex_unlock(&pool->mutex);
            break; // Sair da thread
        }

        // Retirar trabalho da Fila Local
        job_node_t* job = pool->head;
        client_fd = job->client_fd;
        pool->head = job->next;
        if (pool->head == NULL) pool->tail = NULL;
        pool->count--;
        free(job);

        pthread_mutex_unlock(&pool->mutex);
        // -----------------------------------------------

        // Processamento (Igual ao anterior)
        memset(buffer, 0, sizeof(buffer));
        if (read(client_fd, buffer, sizeof(buffer) - 1) > 0) {
            http_request_t req;
            if (parse_http_request(buffer, &req) == 0) {
                // Lógica de Negócio
                if (strstr(req.path, "/api/register")) {
                    char plan = ' ';
                    if (strstr(req.path, "plan=A")) plan = 'A';
                    if (strstr(req.path, "plan=B")) plan = 'B';
                    if (strstr(req.path, "plan=C")) plan = 'C';
                    
                    int success = 0; char* msg = "Esgotado";
                    sem_wait(sems->stats_mutex);
                    if (plan=='A' && data->slots_A>0) { data->slots_A--; success=1; msg="Sucesso!"; }
                    else if (plan=='B' && data->slots_B>0) { data->slots_B--; success=1; msg="Sucesso!"; }
                    else if (plan=='C' && data->slots_C>0) { data->slots_C--; success=1; msg="Sucesso!"; }
                    sem_post(sems->stats_mutex);

                    char json[256];
                    snprintf(json, sizeof(json), "{\"message\": \"%s\", \"success\": %s}", msg, success?"true":"false");
                    send_http_response(client_fd, success?200:409, "OK", "application/json", json, strlen(json));
                }
                else if (strstr(req.path, "/api/slots")) {
                    char json[256];
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
                    if(f) {
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
        if(data->stats.active_connections > 0) data->stats.active_connections--;
        sem_post(sems->stats_mutex);
    }
    return NULL;
}

// Função chamada pelo Dispatcher (Main Thread do Worker)
void thread_pool_submit(thread_pool_t* pool, int client_fd) {
    job_node_t* job = malloc(sizeof(job_node_t));
    job->client_fd = client_fd;
    job->next = NULL;

    pthread_mutex_lock(&pool->mutex);
    
    if (pool->tail) {
        pool->tail->next = job;
        pool->tail = job;
    } else {
        pool->head = job;
        pool->tail = job;
    }
    pool->count++;
    
    pthread_cond_signal(&pool->cond); // Acorda uma thread
    pthread_mutex_unlock(&pool->mutex);
}

thread_pool_t* create_thread_pool(int num_threads, shared_data_t* shm, semaphores_t* sems, int ipc_socket) {
    thread_pool_t* pool = malloc(sizeof(thread_pool_t));
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    pool->num_threads = num_threads;
    pool->shutdown = 0;
    pool->head = NULL;
    pool->tail = NULL;
    pool->count = 0;
    
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond, NULL);

    thread_worker_args_t* args = malloc(sizeof(thread_worker_args_t));
    args->pool = pool;
    args->shared_data = shm;
    args->sems = sems;
    args->ipc_socket = ipc_socket;
    args->local_cache = NULL;
    pool->args = args;

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread, args);
    }
    return pool;
}

void destroy_thread_pool(thread_pool_t* pool) {
    if (!pool) return;
    
    // 1. Avisar threads para saírem
    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->cond); // Acorda todas
    pthread_mutex_unlock(&pool->mutex);

    // 2. Join (Requisito: "Implement proper cleanup on shutdown")
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    // 3. Limpar memória
    if (pool->args) free(pool->args);
    if (pool->threads) free(pool->threads);
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    
    // Limpar jobs pendentes (se houver)
    while (pool->head) {
        job_node_t* tmp = pool->head;
        pool->head = pool->head->next;
        close(tmp->client_fd);
        free(tmp);
    }
    free(pool);
}