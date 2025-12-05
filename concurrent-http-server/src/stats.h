#ifndef STATS_H
#define STATS_H

#include "shared_mem.h"
#include "semaphores.h"

// Inicializa o tempo de arranque
void init_stats(shared_data_t* data);

// Loop que corre numa thread separada no Master para mostrar stats
void* stats_monitor_thread(void* arg);

// Ajuda a calcular tempo decorrido
double get_current_time_ms();

#endif