#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <pthread.h>

typedef struct {
    int fd;                   
    int user_id;
    int room_id;
    pthread_t thread;
    int is_connected;
} WebSocketClient;

typedef struct {
    WebSocketClient clients[1000];
    int client_count;
    pthread_mutex_t clients_mutex;
} WebSocketServer;

// Initialize WebSocket server
int websocket_init(int port);
int websocket_start();
void websocket_stop();
void websocket_cleanup();

// Client management
int websocket_add_client(int fd, int user_id, int room_id);
int websocket_remove_client(int fd);
int websocket_get_client_index(int fd);

// Broadcasting
int websocket_broadcast_to_room(int room_id, const char *message);
int websocket_send_to_client(int fd, const char *message);

// Server operations
int websocket_get_active_connections();

#endif 
