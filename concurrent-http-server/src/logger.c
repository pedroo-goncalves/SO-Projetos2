#include "logger.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

// Implementação baseada na secção 8 do PDF [cite: 589-606]
void log_request(sem_t* log_sem, const char* client_ip, const char* method, 
                 const char* path, int status, size_t bytes) {
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[64];
    
    // Formata a data/hora
    strftime(timestamp, sizeof(timestamp), "%d/%b/%Y:%H:%M:%S %z", tm_info);
    
    // Entra na secção crítica (proteção com semáforo)
    sem_wait(log_sem);
    
    // Abre o ficheiro em modo "append"
    FILE* log = fopen("access.log", "a");
    if (log) {
        // Escreve no formato Apache Combined Log
        fprintf(log, "%s - - [%s] \"%s %s HTTP/1.1\" %d %zu\n", 
                client_ip, timestamp, method, path, status, bytes);
        fclose(log);
    }
    
    // Sai da secção crítica
    sem_post(log_sem);
}