#include "http_server.h"
#include "authentication.h"
#include "database.h"
#include "utils.h"
#include "thread_pool.h"
#include "websocket_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

static int g_http_server_fd = -1;
static int g_http_running = 0;

void http_parse_request(const char *raw_request, HTTPRequest *req) {
    memset(req, 0, sizeof(HTTPRequest));
    
    // Parse request line
    char method[16], path[256], protocol[32];
    sscanf(raw_request, "%s %s %s", method, path, protocol);
    
    strncpy(req->method, method, sizeof(req->method) - 1);
    
    // Parse path and query string
    char *query_pos = strchr(path, '?');
    if (query_pos) {
        strncpy(req->query_string, query_pos + 1, sizeof(req->query_string) - 1);
        *query_pos = '\0';
    }
    strncpy(req->path, path, sizeof(req->path) - 1);
    
    // Parse body (simple approach - find double CRLF)
    const char *body_start = strstr(raw_request, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        strncpy(req->body, body_start, sizeof(req->body) - 1);
        req->content_length = strlen(req->body);
    }
}

// URL decode utility
void http_url_decode(const char *src, char *dest, size_t dest_size) {
    size_t i = 0, j = 0;
    
    while (src[i] && j < dest_size - 1) {
        if (src[i] == '%' && i + 2 < strlen(src)) {
            int value;
            sscanf(src + i + 1, "%2x", &value);
            dest[j++] = (char)value;
            i += 3;
        } else if (src[i] == '+') {
            dest[j++] = ' ';
            i++;
        } else {
            dest[j++] = src[i++];
        }
    }
    dest[j] = '\0';
}

// Parse JSON-like string value (simple implementation)
int http_parse_json_string(const char *json, const char *key, char *value, size_t max_len) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    const char *pos = strstr(json, search_key);
    if (!pos) {
        log_debug("Could not find key: %s", search_key);
        return -1;
    }
    
    pos += strlen(search_key);
    
    // Skip whitespace and colons
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == ':')) pos++;
    
    // Check if value is string
    if (*pos == '"') {
        pos++;
        size_t i = 0;
        while (*pos && *pos != '"' && i < max_len - 1) {
            value[i++] = *pos++;
        }
        value[i] = '\0';
        log_debug("Parsed string value: %s", value);
        return 0;
    }
    
    log_debug("Expected quote but got: %c (ASCII %d)", *pos, *pos);
    return -1;
}

// Parse JSON-like integer value (simple implementation)
int http_parse_json_int(const char *json, const char *key, int *value) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    const char *pos = strstr(json, search_key);
    if (!pos) {
        log_debug("Could not find key: %s", search_key);
        return -1;
    }
    
    pos += strlen(search_key);
    
    // Skip whitespace and colons
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == ':')) pos++;
    
    // Parse integer value
    if (*pos && (isdigit(*pos) || *pos == '-')) {
        *value = atoi(pos);
        log_debug("Parsed integer from position, value: %d", *value);
        return 0;
    }
    
    log_debug("Could not parse integer, char at pos: %c (ASCII %d)", *pos, *pos);
    return -1;
}

// Response creators
HTTPResponse* http_response_create(int status_code, const char *content_type) {
    HTTPResponse *resp = malloc(sizeof(HTTPResponse));
    if (!resp) return NULL;
    
    resp->status_code = status_code;
    strncpy(resp->content_type, content_type, sizeof(resp->content_type) - 1);
    memset(resp->body, 0, sizeof(resp->body));
    
    return resp;
}

void http_response_set_body(HTTPResponse *resp, const char *body) {
    strncpy(resp->body, body, sizeof(resp->body) - 1);
}

void http_response_send(int client_fd, HTTPResponse *resp) {
    const char *status_text = "OK";
    
    if (resp->status_code == 400) status_text = "Bad Request";
    else if (resp->status_code == 401) status_text = "Unauthorized";
    else if (resp->status_code == 404) status_text = "Not Found";
    else if (resp->status_code == 500) status_text = "Internal Server Error";
    
    char header[2048];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
             "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
             "Access-Control-Max-Age: 86400\r\n"
             "Connection: close\r\n"
             "\r\n",
             resp->status_code, status_text, resp->content_type, strlen(resp->body));
    
    send(client_fd, header, strlen(header), 0);
    send(client_fd, resp->body, strlen(resp->body), 0);
}

void http_response_free(HTTPResponse *resp) {
    if (resp) free(resp);
}

// HTTP request handler (will be expanded with actual endpoints)
static void handle_http_request(int client_fd) {
    char buffer[4096] = {0};
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes <= 0) {
        close(client_fd);
        return;
    }
    
    HTTPRequest req = {0};
    http_parse_request(buffer, &req);
    req.client_fd = client_fd;
    
    // Log request details
    log_debug("HTTP Request - Method: %s, Path: %s", req.method, req.path);
    if (strlen(req.body) > 0) {
        log_debug("Request Body: %s", req.body);
    }
    
    HTTPResponse *resp = NULL;


    if (strcmp(req.method, "OPTIONS") == 0) {
        resp = http_response_create(200, "application/json");
        http_response_set_body(resp, "");
        http_response_send(client_fd, resp);
        http_response_free(resp);
        close(client_fd);
        return;
    }
    
    if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/register") == 0) {
        // Register endpoint
        char username[50], password[64], role_str[20];
        
        if (http_parse_json_string(req.body, "username", username, sizeof(username)) < 0 ||
            http_parse_json_string(req.body, "password", password, sizeof(password)) < 0 ||
            http_parse_json_string(req.body, "role", role_str, sizeof(role_str)) < 0) {
            
            resp = http_response_create(400, "application/json");
            http_response_set_body(resp, "{\"status\": \"error\", \"message\": \"Invalid request\"}");
        } else {
            UserRole role = (strcmp(role_str, "admin") == 0) ? ROLE_ADMIN : ROLE_USER;
            int user_id;
            
            if (auth_register(username, password, role, &user_id) == 0) {
                char body[256];
                snprintf(body, sizeof(body), 
                        "{\"status\": \"success\", \"user_id\": %d, \"username\": \"%s\", \"role\": \"%s\"}",
                        user_id, username, role_str);
                
                resp = http_response_create(200, "application/json");
                http_response_set_body(resp, body);
            } else {
                resp = http_response_create(400, "application/json");
                http_response_set_body(resp, "{\"status\": \"error\", \"message\": \"Registration failed\"}");
            }
        }
    } 
    else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/login") == 0) {
        // Login endpoint
        char username[50], password[64];
        
        if (http_parse_json_string(req.body, "username", username, sizeof(username)) < 0 ||
            http_parse_json_string(req.body, "password", password, sizeof(password)) < 0) {
            
            resp = http_response_create(400, "application/json");
            http_response_set_body(resp, "{\"status\": \"error\", \"message\": \"Invalid request\"}");
        } else {
            int user_id;
            
            if (auth_login(username, password, &user_id) == 0) {
                char token[128];
                auth_generate_token(user_id, token, sizeof(token));
                
                User *user = db_get_user_by_id(user_id);
                char body[512];
                snprintf(body, sizeof(body), 
                        "{\"status\": \"success\", \"user_id\": %d, \"username\": \"%s\", \"role\": \"%s\", \"token\": \"%s\"}",
                        user_id, username, user->role == ROLE_ADMIN ? "admin" : "user", token);
                
                resp = http_response_create(200, "application/json");
                http_response_set_body(resp, body);
            } else {
                resp = http_response_create(401, "application/json");
                http_response_set_body(resp, "{\"status\": \"error\", \"message\": \"Invalid credentials\"}");
            }
        }
    }
    else if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/api/rooms") == 0) {
        // List rooms endpoint
        int count;
        ChatRoom *rooms = db_get_all_rooms(&count);
        
        char body[4096] = "{\"status\": \"success\", \"rooms\": [";
        for (int i = 0; i < count; i++) {
            if (i > 0) strcat(body, ",");
            char room_json[512];
            snprintf(room_json, sizeof(room_json),
                    "{\"room_id\": %d, \"room_name\": \"%s\", \"user_count\": %d}",
                    rooms[i].room_id, rooms[i].room_name, rooms[i].current_user_count);
            strcat(body, room_json);
        }
        strcat(body, "]}");
        
        if (rooms) free(rooms);
        
        resp = http_response_create(200, "application/json");
        http_response_set_body(resp, body);
    }
    else if (strcmp(req.method, "GET") == 0 && str_starts_with(req.path, "/api/rooms/")) {
        // Get room users endpoint: /api/rooms/{room_id}/users
        const char *path_part = req.path + strlen("/api/rooms/");
        int room_id = atoi(path_part);
        
        if (room_id <= 0) {
            resp = http_response_create(400, "application/json");
            http_response_set_body(resp, "{\"status\": \"error\", \"message\": \"Invalid room ID\"}");
        } else {
            // Check if path contains /users
            if (strstr(req.path, "/users")) {
                char response_body[2048];
                snprintf(response_body, sizeof(response_body),
                        "{\"status\": \"success\", \"room_id\": %d, \"users\": []}", room_id);
                
                resp = http_response_create(200, "application/json");
                http_response_set_body(resp, response_body);
            } else {
                resp = http_response_create(404, "application/json");
                http_response_set_body(resp, "{\"status\": \"error\", \"message\": \"Endpoint not found\"}");
            }
        }
    }
    else if (strcmp(req.method, "POST") == 0 && str_starts_with(req.path, "/api/rooms/")) {
        // Join room endpoint: /api/rooms/{room_id}/join
        const char *path_part = req.path + strlen("/api/rooms/");
        int room_id = atoi(path_part);
        
        if (room_id <= 0 || !strstr(req.path, "/join")) {
            resp = http_response_create(400, "application/json");
            http_response_set_body(resp, "{\"status\": \"error\", \"message\": \"Invalid room ID\"}");
        } else {
            int user_id;
            if (http_parse_json_int(req.body, "user_id", &user_id) < 0) {
                resp = http_response_create(400, "application/json");
                http_response_set_body(resp, "{\"status\": \"error\", \"message\": \"Missing user_id\"}");
            } else {
                // Add user to room in in-memory database
                int result = db_add_user_to_room(room_id, user_id);
                
                if (result == 0) {
                    char response_body[256];
                    snprintf(response_body, sizeof(response_body),
                            "{\"status\": \"success\", \"room_id\": %d, \"user_id\": %d, \"message\": \"Joined room\"}",
                            room_id, user_id);
                    
                    resp = http_response_create(200, "application/json");
                    http_response_set_body(resp, response_body);
                    printf("[HTTP] User %d successfully joined room %d\n", user_id, room_id);
                } else {
                    char error_msg[256];
                    if (result == -1) {
                        snprintf(error_msg, sizeof(error_msg), "{\"status\": \"error\", \"message\": \"Room is full\", \"error_code\": %d}", result);
                    } else if (result == -2) {
                        snprintf(error_msg, sizeof(error_msg), "{\"status\": \"error\", \"message\": \"User already in room\", \"error_code\": %d}", result);
                    } else if (result == -3) {
                        snprintf(error_msg, sizeof(error_msg), "{\"status\": \"error\", \"message\": \"Room not found (ID: %d)\", \"error_code\": %d}", room_id, result);
                    } else {
                        snprintf(error_msg, sizeof(error_msg), "{\"status\": \"error\", \"message\": \"Failed to join room\", \"error_code\": %d}", result);
                    }
                    
                    resp = http_response_create(400, "application/json");
                    http_response_set_body(resp, error_msg);
                    printf("[HTTP] Failed to add user %d to room %d: error code %d\n", user_id, room_id, result);
                }
            }
        }
    }
    else if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/rooms/create") == 0) {
        // Create room endpoint
        char room_name[100] = {0};
        int user_id = 0;
        
        // Debug logging
        log_debug("Create room request body: %s", req.body);
        
        int room_name_result = http_parse_json_string(req.body, "room_name", room_name, sizeof(room_name));
        
        // Try to parse user_id first, then created_by
        int user_id_result = http_parse_json_int(req.body, "user_id", &user_id);
        if (user_id_result < 0) {
            user_id_result = http_parse_json_int(req.body, "created_by", &user_id);
        }
        
        log_debug("room_name parse result: %d, room_name: %s", room_name_result, room_name);
        log_debug("user_id parse result: %d, user_id: %d", user_id_result, user_id);
        
        if (room_name_result < 0 || user_id_result < 0) {
            log_error("Invalid room creation request - missing room_name or user_id/created_by");
            resp = http_response_create(400, "application/json");
            http_response_set_body(resp, "{\"status\": \"error\", \"message\": \"Invalid request - missing room_name or user_id/created_by\"}");
        } else {
            // Create room in in-memory database
            int room_id = db_create_room(room_name, user_id);
            
            // Add creator to room
            if (room_id > 0) {
                db_add_user_to_room(room_id, user_id);
            }
            
            char body[256];
            snprintf(body, sizeof(body), 
                    "{\"status\": \"success\", \"room_id\": %d, \"room_name\": \"%s\"}",
                    room_id, room_name);
            
            resp = http_response_create(200, "application/json");
            http_response_set_body(resp, body);
        }
    }
    else if (strcmp(req.method, "POST") == 0 && str_starts_with(req.path, "/api/messages/send")) {
        // Send message endpoint
        char message_content[500];
        int user_id, room_id;
        
        if (http_parse_json_string(req.body, "message", message_content, sizeof(message_content)) < 0 ||
            http_parse_json_int(req.body, "user_id", &user_id) < 0 ||
            http_parse_json_int(req.body, "room_id", &room_id) < 0) {
            
            resp = http_response_create(400, "application/json");
            http_response_set_body(resp, "{\"status\": \"error\", \"message\": \"Invalid request - missing message, user_id, or room_id\"}");
        } else {
            // Get user info from in-memory DB for username
            User *user = db_get_user_by_id(user_id);
            if (!user) {
                resp = http_response_create(404, "application/json");
                http_response_set_body(resp, "{\"status\": \"error\", \"message\": \"User not found\"}");
            } else {
                // Store message in in-memory database
                int message_id = db_create_message(user_id, room_id, user->username, message_content);
                
                // Create message JSON for broadcasting
                char message_json[2048];
                snprintf(message_json, sizeof(message_json),
                        "{\"type\": \"message\", \"message_id\": %d, \"user_id\": %d, \"username\": \"%s\", \"content\": \"%s\", \"room_id\": %d}",
                        message_id, user_id, user->username, message_content, room_id);
                
                // Broadcast to WebSocket clients in the same room
                websocket_broadcast_to_room(room_id, message_json);
                
                // Return success response
                char response_body[512];
                snprintf(response_body, sizeof(response_body),
                        "{\"status\": \"success\", \"message_id\": %d, \"message\": \"Message sent successfully\"}",
                        message_id);
                
                resp = http_response_create(200, "application/json");
                http_response_set_body(resp, response_body);
            }
        }
    }
    else if (strcmp(req.method, "GET") == 0 && str_starts_with(req.path, "/api/messages/")) {
        // Get messages for a room endpoint
        // Extract room_id from path like /api/messages/1
        const char *room_id_str = req.path + strlen("/api/messages/");
        int room_id = atoi(room_id_str);
        
        if (room_id <= 0) {
            resp = http_response_create(400, "application/json");
            http_response_set_body(resp, "{\"status\": \"error\", \"message\": \"Invalid room ID\"}");
        } else {
            // Get messages from database (this would need to be implemented in database.c)
            char response_body[2048];
            snprintf(response_body, sizeof(response_body),
                    "{\"status\": \"success\", \"room_id\": %d, \"messages\": []}", room_id);
            
            resp = http_response_create(200, "application/json");
            http_response_set_body(resp, response_body);
        }
    }
    else {
        resp = http_response_create(404, "application/json");
        http_response_set_body(resp, "{\"status\": \"error\", \"message\": \"Endpoint not found\"}");
    }
    
    if (resp) {
        http_response_send(client_fd, resp);
        http_response_free(resp);
    }
    
    close(client_fd);
}

// Task for thread pool
static void http_handler_task(void *arg) {
    int *client_fd_ptr = (int *)arg;
    int client_fd = *client_fd_ptr;
    free(client_fd_ptr);
    
    handle_http_request(client_fd);
}

int http_server_init(int port) {
    g_http_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_http_server_fd < 0) {
        log_error("Failed to create HTTP server socket");
        return -1;
    }
    
    socket_set_reuseaddr(g_http_server_fd);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    
    if (bind(g_http_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("Failed to bind HTTP server to port %d", port);
        return -1;
    }
    
    if (listen(g_http_server_fd, 128) < 0) {
        log_error("Failed to listen on HTTP server");
        return -1;
    }
    
    g_http_running = 1;
    log_info("HTTP server initialized on port %d", port);
    
    return 0;
}

int http_server_start() {
    log_info("HTTP server starting...");
    
    while (g_http_running) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_fd = accept(g_http_server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            log_error("Failed to accept HTTP connection");
            continue;
        }
        
        log_debug("HTTP client connected from %s:%d", 
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        int *client_fd_ptr = malloc(sizeof(int));
        if (client_fd_ptr) {
            *client_fd_ptr = client_fd;
            thread_pool_submit(http_handler_task, client_fd_ptr);
        }
    }
    
    return 0;
}

void http_server_stop() {
    g_http_running = 0;
    if (g_http_server_fd >= 0) {
        close(g_http_server_fd);
        g_http_server_fd = -1;
    }
    log_info("HTTP server stopped");
}

void http_server_cleanup() {
    http_server_stop();
}
