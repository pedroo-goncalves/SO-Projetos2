#ifndef MASTER_H
#define MASTER_H
#include <signal.h>
#include "shared_mem.h"  // Necessário para shared_data_t
#include "semaphores.h"  // Necessário para semaphores_t

// Variável global para controlo do loop principal (definida no master.c)
extern volatile sig_atomic_t keep_running;

// Protótipos das funções
void signal_handler(int signum);
int create_server_socket(int port);
void enqueue_connection(shared_data_t* data, semaphores_t* sems, int client_fd);

#endif