#include "websocket_server.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <ctype.h>

#define MAX_FRAME_SIZE 65536
#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// Global server state
static WebSocketServer g_server = {0};
static int g_server_fd = -1;
static int g_running = 0;
static int g_ws_port = 7070;
static pthread_t g_accept_thread;

// Base64 encoding helper
static void base64_encode(const unsigned char *input, int length, char *output) {
    BIO *bio, *b64;
    BUF_MEM *buffer_ptr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &buffer_ptr);

    memcpy(output, buffer_ptr->data, buffer_ptr->length);
    output[buffer_ptr->length] = '\0';

    BIO_free_all(bio);
}

// WebSocket key acceptance header generator
static void generate_accept_header(const char *key, char *accept) {
    unsigned char hash[20];
    char concat[256];
    
    snprintf(concat, sizeof(concat), "%s%s", key, WS_GUID);
    
    SHA1((unsigned char*)concat, strlen(concat), hash);
    base64_encode(hash, 20, accept);
}

// Parse WebSocket handshake request
static int parse_websocket_handshake(const char *request, char *key) {
    const char *line_start = request;
    const char *line_end;
    
    while (*line_start) {
        line_end = strstr(line_start, "\r\n");
        if (!line_end) {
            line_end = line_start + strlen(line_start);
        }
        
        // Look for Sec-WebSocket-Key header
        if (strncasecmp(line_start, "Sec-WebSocket-Key:", 18) == 0) {
            const char *val_start = line_start + 18;
            // Skip whitespace
            while (val_start < line_end && isspace(*val_start)) {
                val_start++;
            }
            int len = line_end - val_start;
            strncpy(key, val_start, len);
            key[len] = '\0';
            return 1;
        }
        
        if (line_end[0] == '\0') break;
        line_start = line_end + 2; // Skip \r\n
    }
    
    return 0;
}

// WebSocket frame parsing
typedef struct {
    int fin;
    int opcode;
    int masked;
    int payload_len;
    unsigned char mask[4];
    unsigned char *payload;
} WSFrame;

static int parse_ws_frame(const unsigned char *data, int data_len, WSFrame *frame) {
    if (data_len < 2) return -1;
    
    frame->fin = (data[0] & 0x80) >> 7;
    frame->opcode = data[0] & 0x0F;
    frame->masked = (data[1] & 0x80) >> 7;
    frame->payload_len = data[1] & 0x7F;
    
    int header_size = 2;
    
    // Extended payload length
    if (frame->payload_len == 126) {
        if (data_len < 4) return -1;
        frame->payload_len = (data[2] << 8) | data[3];
        header_size = 4;
    } else if (frame->payload_len == 127) {
        if (data_len < 10) return -1;
        frame->payload_len = 0;
        for (int i = 0; i < 8; i++) {
            frame->payload_len = (frame->payload_len << 8) | data[2 + i];
        }
        header_size = 10;
    }
    
    // Masking key
    if (frame->masked) {
        if (data_len < header_size + 4) return -1;
        memcpy(frame->mask, &data[header_size], 4);
        header_size += 4;
    }
    
    // Check if we have full payload
    if (data_len < header_size + frame->payload_len) return -1;
    
    frame->payload = (unsigned char*)malloc(frame->payload_len);
    if (!frame->payload) return -1;
    
    memcpy(frame->payload, &data[header_size], frame->payload_len);
    
    // Unmask payload if masked
    if (frame->masked) {
        for (int i = 0; i < frame->payload_len; i++) {
            frame->payload[i] ^= frame->mask[i % 4];
        }
    }
    
    return header_size + frame->payload_len;
}

// Create WebSocket frame for sending
static int create_ws_frame(const char *data, int data_len, unsigned char *frame, int frame_size) {
    if (frame_size < 2 + data_len) return -1;
    
    int pos = 0;
    
    // FIN + opcode (text frame = 1)
    frame[pos++] = 0x81;
    
    // Payload length
    if (data_len < 126) {
        frame[pos++] = data_len & 0x7F;
    } else if (data_len < 65536) {
        frame[pos++] = 126;
        frame[pos++] = (data_len >> 8) & 0xFF;
        frame[pos++] = data_len & 0xFF;
    } else {
        frame[pos++] = 127;
        for (int i = 7; i >= 0; i--) {
            frame[pos++] = (data_len >> (i * 8)) & 0xFF;
        }
    }
    
    // Payload
    memcpy(&frame[pos], data, data_len);
    pos += data_len;
    
    return pos;
}

// Client handler thread
void* websocket_client_handler(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);
    
    unsigned char buffer[MAX_FRAME_SIZE];
    char handshake_response[1024];
    char key[256] = {0};
    char accept[256] = {0};
    
    // Read handshake
    ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return NULL;
    }
    
    buffer[n] = '\0';
    
    // Parse and generate handshake response
    if (!parse_websocket_handshake((const char*)buffer, key)) {
        log_error("Failed to parse WebSocket key");
        close(client_fd);
        return NULL;
    }
    
    generate_accept_header(key, accept);
    
    snprintf(handshake_response, sizeof(handshake_response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept);
    
    if (send(client_fd, handshake_response, strlen(handshake_response), 0) < 0) {
        log_error("Failed to send handshake response");
        close(client_fd);
        return NULL;
    }
    
    log_info("WebSocket client connected: fd=%d", client_fd);
    
    // Add client to server
    int client_idx = websocket_add_client(client_fd, 0, 0);  // Default user_id=0, room_id=0
    if (client_idx < 0) {
        log_error("Failed to add client to server");
        close(client_fd);
        return NULL;
    }
    
    // Main message loop
    while (g_running) {
        n = recv(client_fd, buffer, sizeof(buffer), 0);
        
        if (n <= 0) {
            if (n < 0) {
                log_error("Error receiving from client: %s", strerror(errno));
            }
            break;
        }
        
        // Parse WebSocket frame
        WSFrame frame = {0};
        int frame_size = parse_ws_frame(buffer, n, &frame);
        
        if (frame_size < 0) {
            log_error("Failed to parse WebSocket frame");
            break;
        }
        
        // Handle different opcodes
        if (frame.opcode == 0x1) {  // Text frame
            char message[4096];
            if (frame.payload_len < sizeof(message)) {
                memcpy(message, frame.payload, frame.payload_len);
                message[frame.payload_len] = '\0';
                
                log_info("Received from client %d: %s", client_fd, message);
                
                // Broadcast to room (for now, broadcast to all)
                pthread_mutex_lock(&g_server.clients_mutex);
                for (int i = 0; i < g_server.client_count; i++) {
                    if (g_server.clients[i].is_connected && g_server.clients[i].fd != client_fd) {
                        int resp_size = create_ws_frame(message, frame.payload_len, buffer, sizeof(buffer));
                        if (resp_size > 0) {
                            send(g_server.clients[i].fd, buffer, resp_size, 0);
                        }
                    }
                }
                pthread_mutex_unlock(&g_server.clients_mutex);
            }
        } else if (frame.opcode == 0x8) {  // Close frame
            log_info("Client %d sent close frame", client_fd);
            free(frame.payload);
            break;
        } else if (frame.opcode == 0x9) {  // Ping frame
            // Send pong (opcode 0xA)
            unsigned char pong_frame[2] = {0x8A, 0x00};
            send(client_fd, pong_frame, 2, 0);
        }
        
        if (frame.payload) {
            free(frame.payload);
        }
    }
    
    // Remove client
    websocket_remove_client(client_fd);
    close(client_fd);
    log_info("WebSocket client disconnected: fd=%d", client_fd);
    
    return NULL;
}

// Accept connections thread
void* websocket_accept_connections(void *arg) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    log_info("WebSocket server accepting connections on port %d", g_ws_port);
    
    while (g_running) {
        int client_fd = accept(g_server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (client_fd < 0) {
            if (g_running) {
                log_error("Failed to accept connection: %s", strerror(errno));
            }
            continue;
        }
        
        log_info("New WebSocket connection from %s:%d", 
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // Create thread to handle client
        pthread_t client_thread;
        int *client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        
        if (pthread_create(&client_thread, NULL, websocket_client_handler, client_fd_ptr) != 0) {
            log_error("Failed to create client handler thread");
            free(client_fd_ptr);
            close(client_fd);
        } else {
            pthread_detach(client_thread);
        }
    }
    
    return NULL;
}

// Initialize WebSocket server
int websocket_init(int port) {
    g_ws_port = port;
    
    // Initialize server structure
    g_server.client_count = 0;
    pthread_mutex_init(&g_server.clients_mutex, NULL);
    
    // Create listening socket
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) {
        log_error("Failed to create WebSocket server socket: %s", strerror(errno));
        return -1;
    }
    
    // Set socket options
    int reuse = 1;
    if (setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        log_error("Failed to set socket options: %s", strerror(errno));
        close(g_server_fd);
        return -1;
    }
    
    // Bind to port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    
    if (bind(g_server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_error("Failed to bind WebSocket server to port %d: %s", port, strerror(errno));
        close(g_server_fd);
        return -1;
    }
    
    // Listen for connections
    if (listen(g_server_fd, 128) < 0) {
        log_error("Failed to listen on WebSocket server: %s", strerror(errno));
        close(g_server_fd);
        return -1;
    }
    
    log_info("WebSocket server initialized on port %d", port);
    return 0;
}

// Start WebSocket server
int websocket_start() {
    g_running = 1;
    
    if (pthread_create(&g_accept_thread, NULL, websocket_accept_connections, NULL) != 0) {
        log_error("Failed to create accept thread");
        return -1;
    }
    
    pthread_join(g_accept_thread, NULL);
    return 0;
}

// Stop WebSocket server
void websocket_stop() {
    g_running = 0;
    
    if (g_server_fd >= 0) {
        close(g_server_fd);
        g_server_fd = -1;
    }
    
    log_info("WebSocket server stopped");
}

// Cleanup WebSocket server
void websocket_cleanup() {
    pthread_mutex_destroy(&g_server.clients_mutex);
    log_info("WebSocket server cleaned up");
}

// Add client to server
int websocket_add_client(int fd, int user_id, int room_id) {
    pthread_mutex_lock(&g_server.clients_mutex);
    
    if (g_server.client_count >= 1000) {
        pthread_mutex_unlock(&g_server.clients_mutex);
        return -1;
    }
    
    int idx = g_server.client_count++;
    g_server.clients[idx].fd = fd;
    g_server.clients[idx].user_id = user_id;
    g_server.clients[idx].room_id = room_id;
    g_server.clients[idx].is_connected = 1;
    
    pthread_mutex_unlock(&g_server.clients_mutex);
    
    log_info("Client added: fd=%d, user_id=%d, room_id=%d", fd, user_id, room_id);
    return idx;
}

// Remove client from server
int websocket_remove_client(int fd) {
    pthread_mutex_lock(&g_server.clients_mutex);
    
    int idx = websocket_get_client_index(fd);
    if (idx >= 0) {
        g_server.clients[idx].is_connected = 0;
        // Shift remaining clients
        for (int i = idx; i < g_server.client_count - 1; i++) {
            g_server.clients[i] = g_server.clients[i + 1];
        }
        g_server.client_count--;
    }
    
    pthread_mutex_unlock(&g_server.clients_mutex);
    
    return idx;
}

// Get client index by file descriptor
int websocket_get_client_index(int fd) {
    for (int i = 0; i < g_server.client_count; i++) {
        if (g_server.clients[i].fd == fd && g_server.clients[i].is_connected) {
            return i;
        }
    }
    return -1;
}

// Broadcast message to room
int websocket_broadcast_to_room(int room_id, const char *message) {
    if (!message) return -1;
    
    int msg_len = strlen(message);
    unsigned char frame[MAX_FRAME_SIZE];
    int frame_size = create_ws_frame(message, msg_len, frame, sizeof(frame));
    
    if (frame_size < 0) return -1;
    
    pthread_mutex_lock(&g_server.clients_mutex);
    
    int sent_count = 0;
    for (int i = 0; i < g_server.client_count; i++) {
        if (g_server.clients[i].is_connected && g_server.clients[i].room_id == room_id) {
            if (send(g_server.clients[i].fd, frame, frame_size, 0) > 0) {
                sent_count++;
            }
        }
    }
    
    pthread_mutex_unlock(&g_server.clients_mutex);
    
    return sent_count;
}

// Send message to specific client
int websocket_send_to_client(int fd, const char *message) {
    if (!message) return -1;
    
    int msg_len = strlen(message);
    unsigned char frame[MAX_FRAME_SIZE];
    int frame_size = create_ws_frame(message, msg_len, frame, sizeof(frame));
    
    if (frame_size < 0) return -1;
    
    if (send(fd, frame, frame_size, 0) > 0) {
        return 0;
    }
    
    return -1;
}

// Get active connections count
int websocket_get_active_connections() {
    int count;
    pthread_mutex_lock(&g_server.clients_mutex);
    count = g_server.client_count;
    pthread_mutex_unlock(&g_server.clients_mutex);
    return count;
}
