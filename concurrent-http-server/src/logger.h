#ifndef LOGGER_H
#define LOGGER_H

#include <semaphore.h>
#include <stddef.h> // para size_t

// Protótipo da função de log
void log_request(sem_t* log_sem, const char* client_ip, const char* method, const char* path, int status, size_t bytes);

#endif