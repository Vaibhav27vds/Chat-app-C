#include "database.h"
#include "authentication.h"
#include "http_server.h"
#include "websocket_server.h"
#include "thread_pool.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

// Global control
static int g_running = 1;

// Signal handler
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        log_info("Shutdown signal received");
        g_running = 0;
    }
}

// HTTP server thread
void* http_server_thread(void *arg) {
    log_info("HTTP server thread started");
    http_server_start();
    return NULL;
}

// WebSocket server thread
void* websocket_server_thread(void *arg) {
    log_info("WebSocket server thread started");
    websocket_start();
    return NULL;
}

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║          Chat Server (C Backend)                       ║\n");
    printf("║          HTTP Server: port 3005                        ║\n");
    printf("║          WebSocket Server: port 7070                   ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize components
    log_info("Initializing chat server...");
    
    db_init();
    thread_pool_init(10, 100);  // 10 worker threads, 100 queue size
    
    // Create some test data in in-memory database
    int user1, user2, admin1;
    auth_register("alice", "password123", ROLE_USER, &user1);
    auth_register("bob", "password123", ROLE_USER, &user2);
    auth_register("admin", "admin123", ROLE_ADMIN, &admin1);
    
    // Create some rooms
    int room1 = db_create_room("General Chat", admin1);
    int room2 = db_create_room("Tech Discussion", user1);
    
    // Add users to rooms
    if (room1 > 0) {
        db_add_user_to_room(room1, user1);
        db_add_user_to_room(room1, user2);
    }
    if (room2 > 0) {
        db_add_user_to_room(room2, user1);
    }
    
    // Print initial state
    db_print_stats();
    db_print_users();
    db_print_rooms();
    
    // Initialize HTTP server
    if (http_server_init(3005) < 0) {
        log_error("Failed to initialize HTTP server");
        return 1;
    }
    
    // Initialize WebSocket server
    if (websocket_init(7070) < 0) {
        log_error("Failed to initialize WebSocket server");
        return 1;
    }
    
    // Start servers in separate threads
    pthread_t http_thread, ws_thread;
    
    if (pthread_create(&http_thread, NULL, http_server_thread, NULL) != 0) {
        log_error("Failed to create HTTP server thread");
        return 1;
    }
    
    if (pthread_create(&ws_thread, NULL, websocket_server_thread, NULL) != 0) {
        log_error("Failed to create WebSocket server thread");
        return 1;
    }
    
    log_info("All servers started successfully!");
    printf("\n💡 Server is running. Press Ctrl+C to shutdown.\n\n");
    
    // Main loop - monitor and wait for shutdown signal
    while (g_running) {
        sleep(1);
        
        // Periodic stats (every 10 seconds)
        static int tick = 0;
        if (++tick % 10 == 0) {
            db_print_stats();
        }
    }
    
    // Shutdown
    log_info("Starting shutdown sequence...");
    
    http_server_stop();
    websocket_stop();
    
    // Wait for threads to finish (with timeout)
    pthread_join(http_thread, NULL);
    pthread_join(ws_thread, NULL);
    
    // Cleanup
    thread_pool_shutdown();
    thread_pool_cleanup();
    websocket_cleanup();
    http_server_cleanup();
    db_cleanup();
    
    log_info("Server shutdown complete!");
    printf("\n");
    
    return 0;
}
