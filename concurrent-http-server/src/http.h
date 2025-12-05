// src/http.h
#ifndef HTTP_H
#define HTTP_H

#include <stddef.h> // Para size_t

typedef struct {
    char method[16];
    char path[512];
    char version[16];
} http_request_t;

// Apenas os protótipos das funções (com ponto e vírgula no fim)
int parse_http_request(const char* buffer, http_request_t* req);
void send_http_response(int fd, int status, const char* status_msg, const char* content_type, const char* body, size_t body_len);

#endif