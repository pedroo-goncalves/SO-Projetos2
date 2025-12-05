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
    data->stats.total_response_time_ms = 0;
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
        // Espera 30 segundos (Requisito do PDF)
        sleep(30);

        sem_wait(sems->stats_mutex);
        
        time_t now = time(NULL);
        double uptime = difftime(now, data->stats.server_start_time);
        
        double avg_time = 0;
        if (data->stats.total_requests > 0) {
            avg_time = data->stats.total_response_time_ms / data->stats.total_requests;
        }

        printf("\n=== SERVER STATISTICS ===\n");
        printf("Uptime: %.0f seconds\n", uptime);
        printf("Total Requests: %ld\n", data->stats.total_requests);
        printf("Bytes Transferred: %ld\n", data->stats.bytes_transferred);
        printf("Active Connections: %d\n", data->stats.active_connections);
        printf("Avg Response Time: %.2f ms\n", avg_time);
        printf("Status Codes: [200: %ld] [404: %ld] [500: %ld]\n", 
               data->stats.status_200, data->stats.status_404, data->stats.status_500);
        printf("Vagas Ginásio: [A: %d] [B: %d] [C: %d]\n", 
               data->slots_A, data->slots_B, data->slots_C);
        printf("=========================\n\n");
        fflush(stdout);

        sem_post(sems->stats_mutex);
    }
    return NULL;
}