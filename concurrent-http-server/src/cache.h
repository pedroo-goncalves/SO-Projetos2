#ifndef CACHE_H
#define CACHE_H

#include <stddef.h>
#include <pthread.h>

// Estrutura de um ficheiro em cache
typedef struct cache_entry {
    char* path;                 // Nome do ficheiro (Key)
    void* data;                 // Conteúdo do ficheiro
    size_t size;                // Tamanho
    struct cache_entry* prev;   // Ponteiro para anterior (LRU)
    struct cache_entry* next;   // Ponteiro para seguinte (LRU)
} cache_entry_t;

// Estrutura da Cache
typedef struct {
    cache_entry_t* head;        // Mais recente
    cache_entry_t* tail;        // Menos recente (candidato a eviction)
    size_t current_size;        // Tamanho atual em bytes
    size_t max_size;            // Limite (10MB)
    pthread_rwlock_t rwlock;    // O segredo: Reader-Writer Lock
} cache_t;

// API
cache_t* cache_init(size_t max_size);
void cache_destroy(cache_t* cache);

// Tenta obter um ficheiro da cache (Retorna cópia segura dos dados)
void* cache_get(cache_t* cache, const char* path, size_t* out_size);

// Insere ficheiro na cache (Gerindo LRU e Eviction)
void cache_put(cache_t* cache, const char* path, const void* data, size_t size);

#endif