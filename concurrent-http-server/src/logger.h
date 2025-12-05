#ifndef LOGGER_H
#define LOGGER_H

#include <semaphore.h>
#include <stddef.h> 

// Escreve uma entrada no access.log protegida por semáforo
void log_request(sem_t* log_sem, int client_fd, const char* method, const char* path, int status, size_t bytes);

#endif