#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

cache_t* cache_init(size_t max_size) {
    cache_t* c = malloc(sizeof(cache_t));
    if (!c) return NULL;
    c->head = NULL;
    c->tail = NULL;
    c->current_size = 0;
    c->max_size = max_size;
    pthread_rwlock_init(&c->rwlock, NULL); // Inicializa RWLock
    return c;
}

void cache_destroy(cache_t* cache) {
    if (!cache) return;
    pthread_rwlock_wrlock(&cache->rwlock);
    
    cache_entry_t* curr = cache->head;
    while (curr) {
        cache_entry_t* next = curr->next;
        free(curr->path);
        free(curr->data);
        free(curr);
        curr = next;
    }
    
    pthread_rwlock_unlock(&cache->rwlock);
    pthread_rwlock_destroy(&cache->rwlock);
    free(cache);
}

// GET: Leitura (Permite concorrência)
void* cache_get(cache_t* cache, const char* path, size_t* out_size) {
    void* data_copy = NULL;
    
    // Bloqueio de LEITURA (rdlock)
    pthread_rwlock_rdlock(&cache->rwlock);
    
    cache_entry_t* curr = cache->head;
    while (curr) {
        if (strcmp(curr->path, path) == 0) {
            // HIT!
            data_copy = malloc(curr->size);
            if (data_copy) {
                memcpy(data_copy, curr->data, curr->size);
                *out_size = curr->size;
            }
            break;
        }
        curr = curr->next;
    }
    
    pthread_rwlock_unlock(&cache->rwlock);
    return data_copy;
}

// PUT: Escrita (Exclusivo)
void cache_put(cache_t* cache, const char* path, const void* data, size_t size) {
    if (size > 1024 * 1024) return; // Não guarda ficheiros > 1MB

    // Bloqueio de ESCRITA (wrlock)
    pthread_rwlock_wrlock(&cache->rwlock);

    // 1. Verificar se já existe
    cache_entry_t* curr = cache->head;
    while (curr) {
        if (strcmp(curr->path, path) == 0) {
            pthread_rwlock_unlock(&cache->rwlock);
            return; 
        }
        curr = curr->next;
    }

    // 2. Eviction Policy (LRU)
    while (cache->current_size + size > cache->max_size && cache->tail) {
        cache_entry_t* old = cache->tail;
        if (old->prev) old->prev->next = NULL;
        else cache->head = NULL;
        
        cache->tail = old->prev;
        
        cache->current_size -= old->size;
        free(old->path);
        free(old->data);
        free(old);
    }

    // 3. Inserir no topo
    if (cache->current_size + size <= cache->max_size) {
        cache_entry_t* new_entry = malloc(sizeof(cache_entry_t));
        if (new_entry) {
            new_entry->path = strdup(path);
            new_entry->data = malloc(size);
            if (new_entry->data) {
                memcpy(new_entry->data, data, size);
                new_entry->size = size;
                
                new_entry->prev = NULL;
                new_entry->next = cache->head;
                
                if (cache->head) cache->head->prev = new_entry;
                cache->head = new_entry;
                
                if (!cache->tail) cache->tail = new_entry;
                
                cache->current_size += size;
            } else {
                free(new_entry->path);
                free(new_entry);
            }
        }
    }

    pthread_rwlock_unlock(&cache->rwlock);
}