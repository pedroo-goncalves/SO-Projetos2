// config.h
#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    
    int port;
    char document_root[256];
    int num_workers;
    int threads_per_worker;
    int max_queue_size;
    char log_file[256];
    int cache_size_mb;
    int timeout_seconds;

    //Novos campos para opções de linha de comandos
    int is_daemon;
    int is_verbose;
    char config_path[256];

} server_config_t;

int load_config(const char* filename, server_config_t* config);

#endif