// src/worker.h
#ifndef WORKER_H
#define WORKER_H

#include "shared_mem.h"
#include "semaphores.h"
#include "config.h"

// Função principal do processo Worker
void worker_main(shared_data_t* shm, semaphores_t* sems, int listen_fd);
#endif