#include "stats.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

typedef struct {
    shared_data_t* data;
    semaphores_t* sems;
} monitor_args_t;

void init_stats(shared_data_t* data) {
    data->stats.server_start_time = time(NULL);
    data->stats.total_requests = 0;
    data->stats.bytes_transferred = 0;
    data->stats.status_200 = 0;
    data->stats.status_404 = 0;
    data->stats.status_500 = 0;
    data->stats.active_connections = 0;
    data->stats.total_response_time_ms = 0;
    data->stats.cache_hits = 0;
    data->stats.cache_misses = 0;
}

double get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

void* stats_monitor_thread(void* arg) {
    monitor_args_t* args = (monitor_args_t*)arg;
    shared_data_t* data = args->data;
    semaphores_t* sems = args->sems;

    while (1) {
        sleep(10); // Atualiza a cada 10s (ou 30s como preferires)

        sem_wait(sems->stats_mutex);
        
        time_t now = time(NULL);
        double uptime = difftime(now, data->stats.server_start_time);
        
        double avg_time = 0;
        if (data->stats.total_requests > 0) {
            avg_time = data->stats.total_response_time_ms / data->stats.total_requests;
        }

        // Cálculo da Cache Hit Rate
        double cache_rate = 0.0;
        long total_cache_ops = data->stats.cache_hits + data->stats.cache_misses;
        if (total_cache_ops > 0) {
            cache_rate = ((double)data->stats.cache_hits / total_cache_ops) * 100.0;
        }

        // Prevenção visual de negativos (caso raro)
        int active = data->stats.active_connections;
        if (active < 0) active = 0;

        // Impressão Formatada (Exatamente como pedido)
        printf("\n========================================\n");
        printf("SERVER STATISTICS\n");
        printf("========================================\n");
        printf("Uptime: %.0f seconds\n", uptime);
        printf("Total Requests: %ld\n", data->stats.total_requests);
        printf("Successful (2xx): %ld\n", data->stats.status_200);
        printf("Client Errors (4xx): %ld\n", data->stats.status_404);
        printf("Server Errors (5xx): %ld\n", data->stats.status_500);
        printf("Bytes Transferred: %ld\n", data->stats.bytes_transferred);
        printf("Average Response Time: %.1f ms\n", avg_time);
        printf("Active Connections: %d\n", active);
        printf("Cache Hit Rate: %.1f%%\n", cache_rate);
        printf("========================================\n");
        
        fflush(stdout);
        sem_post(sems->stats_mutex);
    }
    return NULL;
}