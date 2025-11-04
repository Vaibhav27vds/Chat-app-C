#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <time.h>


typedef struct {
    char method[16];           // GET, POST, PUT, DELETE
    char path[256];            // Request path
    char query_string[512];    // Query parameters
    char body[4096];           // Request body
    int content_length;
    int client_fd;
} HTTPRequest;

typedef struct {
    int status_code;
    char content_type[64];
    char body[8192];
} HTTPResponse;

int http_server_init(int port);
int http_server_start();
void http_server_stop();
void http_server_cleanup();

HTTPResponse* http_response_create(int status_code, const char *content_type);
void http_response_set_body(HTTPResponse *resp, const char *body);
void http_response_send(int client_fd, HTTPResponse *resp);
void http_response_free(HTTPResponse *resp);

void http_parse_request(const char *raw_request, HTTPRequest *req);
void http_url_decode(const char *src, char *dest, size_t dest_size);
int http_parse_json_string(const char *json, const char *key, char *value, size_t max_len);

#endif 
