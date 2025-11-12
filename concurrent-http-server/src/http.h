#include <string.h>
#include <stdio.h>

typedef struct {

    char method[16];
    char path[512];
    char version[16];

} http_request_t;

int parse_http_request(const char* buffer, http_request_t* req) {

    const char* line_end = strstr(buffer, "\r\n");
    
    if (!line_end) return -1;
    
    char first_line[1024];
    
    size_t len = line_end - buffer;
    
    strncpy(first_line, buffer, len);
    
    first_line[len] = '\0';
    
    if (sscanf(first_line, "%s %s %s", req->method, req->path, req->version) != 3) {
        return -1;
    }
    
    return 0;

}