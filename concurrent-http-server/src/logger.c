#include "logger.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define LOG_FILE "access.log"
#define MAX_LOG_SIZE (10 * 1024 * 1024) // 10 MB

void log_request(sem_t* log_sem, int client_fd, const char* method, const char* path, int status, size_t bytes) {
    // 1. Obter Data/Hora Formatada
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%d/%b/%Y:%H:%M:%S %z", tm_info);

    // 2. Obter IP do Cliente
    char client_ip[INET_ADDRSTRLEN] = "0.0.0.0";
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    if (getpeername(client_fd, (struct sockaddr *)&addr, &addr_size) == 0) {
        inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));
    }

    // --- ZONA CRÍTICA (Escrita no Ficheiro) ---
    sem_wait(log_sem);

    // 3. Verificar Rotação de Logs (Se > 10MB)
    struct stat st;
    if (stat(LOG_FILE, &st) == 0) {
        if (st.st_size >= MAX_LOG_SIZE) {
            char backup_name[64];
            snprintf(backup_name, sizeof(backup_name), "%s.old", LOG_FILE);
            rename(LOG_FILE, backup_name); // access.log -> access.log.old
        }
    }

    // 4. Escrever no Log (Append Mode)
    FILE* fp = fopen(LOG_FILE, "a");
    if (fp) {
        // Formato: IP - - [Data] "METHOD /path HTTP/1.1" Status Bytes
        fprintf(fp, "%s - - [%s] \"%s %s HTTP/1.1\" %d %zu\n", 
                client_ip, timestamp, method, path, status, bytes);
        fclose(fp); // Flush imediato
    } else {
        perror("Erro ao abrir log file");
    }

    sem_post(log_sem);
    // ------------------------------------------
}