#ifndef SHARED_MEM_H
#define SHARED_MEM_H
#include <time.h>

#define MAX_QUEUE_SIZE 100

typedef struct {
    long total_requests;
    long bytes_transferred;
    long status_200;
    long status_404;
    long status_500;
    int active_connections;
    
    // Novos campos para Feature 3
    double total_response_time_ms; // Acumulador de tempo (para calcular média)
    time_t server_start_time;      // Para calcular Uptime
} server_stats_t;

typedef struct {
    int sockets[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int count;
} connection_queue_t;

typedef struct {
    connection_queue_t queue;
    server_stats_t stats;
    
    // Vagas do Ginásio
    int slots_A; 
    int slots_B;
    int slots_C;
} shared_data_t;

shared_data_t* create_shared_memory();
void destroy_shared_memory(shared_data_t* data);

#endif