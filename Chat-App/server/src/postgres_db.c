#include "postgres_db.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global connection pool
static ConnectionPool g_pool = {0};

// Initialize connection pool
int pg_init_connection_pool(const char *host, const char *port, 
                            const char *database, const char *user, 
                            const char *password, int pool_size) {
    pthread_mutex_init(&g_pool.mutex, NULL);
    
    g_pool.connections = (PooledConnection *)malloc(pool_size * sizeof(PooledConnection));
    if (!g_pool.connections) {
        log_error("Failed to allocate connection pool");
        return -1;
    }
    
    g_pool.pool_size = pool_size;
    
    // Build connection string
    char conninfo[512];
    snprintf(conninfo, sizeof(conninfo),
             "host=%s port=%s dbname=%s user=%s password=%s connect_timeout=10 sslmode=disable",
             host, port, database, user, password);
    
    // Create connections
    for (int i = 0; i < pool_size; i++) {
        PGconn *conn = PQconnectdb(conninfo);
        
        if (PQstatus(conn) != CONNECTION_OK) {
            log_error("Connection to PostgreSQL failed: %s", PQerrorMessage(conn));
            PQfinish(conn);
            return -1;
        }
        
        g_pool.connections[i].conn = conn;
        g_pool.connections[i].in_use = 0;
    }
    
    log_info("PostgreSQL connection pool initialized with %d connections", pool_size);
    return 0;
}

// Get connection from pool
PGconn* pg_get_connection() {
    pthread_mutex_lock(&g_pool.mutex);
    
    for (int i = 0; i < g_pool.pool_size; i++) {
        if (!g_pool.connections[i].in_use) {
            g_pool.connections[i].in_use = 1;
            pthread_mutex_unlock(&g_pool.mutex);
            return g_pool.connections[i].conn;
        }
    }
    
    pthread_mutex_unlock(&g_pool.mutex);
    log_error("No available connections in pool");
    return NULL;
}

// Return connection to pool
void pg_return_connection(PGconn *conn) {
    pthread_mutex_lock(&g_pool.mutex);
    
    for (int i = 0; i < g_pool.pool_size; i++) {
        if (g_pool.connections[i].conn == conn) {
            g_pool.connections[i].in_use = 0;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_pool.mutex);
}

// Cleanup connection pool
void pg_cleanup_connection_pool() {
    pthread_mutex_lock(&g_pool.mutex);
    
    for (int i = 0; i < g_pool.pool_size; i++) {
        if (g_pool.connections[i].conn) {
            PQfinish(g_pool.connections[i].conn);
        }
    }
    
    free(g_pool.connections);
    pthread_mutex_unlock(&g_pool.mutex);
    pthread_mutex_destroy(&g_pool.mutex);
    
    log_info("PostgreSQL connection pool cleaned up");
}

// ============= USER OPERATIONS =============

int pg_create_user(const char *username, const char *password_hash, int role) {
    PGconn *conn = pg_get_connection();
    if (!conn) return -1;
    
    const char *query = "INSERT INTO users (username, password_hash, role) VALUES ($1, $2, $3) RETURNING user_id;";
    const char *params[3] = {username, password_hash, NULL};
    char role_str[16];
    snprintf(role_str, sizeof(role_str), "%d", role);
    params[2] = role_str;
    
    PGresult *res = PQexecParams(conn, query, 3, NULL, params, NULL, NULL, 0);
    int user_id = -1;
    
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        user_id = atoi(PQgetvalue(res, 0, 0));
        log_info("User created: %s (ID: %d)", username, user_id);
    } else {
        log_error("Failed to create user: %s", PQerrorMessage(conn));
    }
    
    PQclear(res);
    pg_return_connection(conn);
    return user_id;
}

int pg_get_user_by_username(const char *username, int *user_id, int *role) {
    PGconn *conn = pg_get_connection();
    if (!conn) return -1;
    
    const char *query = "SELECT user_id, role FROM users WHERE username = $1;";
    const char *params[1] = {username};
    
    PGresult *res = PQexecParams(conn, query, 1, NULL, params, NULL, NULL, 0);
    int result = -1;
    
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        if (PQntuples(res) > 0) {
            *user_id = atoi(PQgetvalue(res, 0, 0));
            *role = atoi(PQgetvalue(res, 0, 1));
            result = 0;
        }
    } else {
        log_error("Failed to get user: %s", PQerrorMessage(conn));
    }
    
    PQclear(res);
    pg_return_connection(conn);
    return result;
}

int pg_user_exists(const char *username) {
    int user_id, role;
    return pg_get_user_by_username(username, &user_id, &role) == 0 ? 1 : 0;
}

int pg_update_user_online_status(int user_id, int is_online) {
    PGconn *conn = pg_get_connection();
    if (!conn) return -1;
    
    char user_id_str[16], is_online_str[2];
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    snprintf(is_online_str, sizeof(is_online_str), "%d", is_online);
    
    const char *query = "UPDATE users SET is_online = $1 WHERE user_id = $2;";
    const char *params[2] = {is_online_str, user_id_str};
    
    PGresult *res = PQexecParams(conn, query, 2, NULL, params, NULL, NULL, 0);
    int result = (PQresultStatus(res) == PGRES_COMMAND_OK) ? 0 : -1;
    
    PQclear(res);
    pg_return_connection(conn);
    return result;
}

// ============= CHAT ROOM OPERATIONS =============

int pg_create_room(const char *room_name, int created_by) {
    PGconn *conn = pg_get_connection();
    if (!conn) return -1;
    
    char created_by_str[16];
    snprintf(created_by_str, sizeof(created_by_str), "%d", created_by);
    
    const char *query = "INSERT INTO chat_rooms (room_name, created_by) VALUES ($1, $2) RETURNING room_id;";
    const char *params[2] = {room_name, created_by_str};
    
    PGresult *res = PQexecParams(conn, query, 2, NULL, params, NULL, NULL, 0);
    int room_id = -1;
    
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        room_id = atoi(PQgetvalue(res, 0, 0));
        log_info("Room created: %s (ID: %d)", room_name, room_id);
    }
    
    PQclear(res);
    pg_return_connection(conn);
    return room_id;
}

int pg_add_user_to_room(int room_id, int user_id) {
    PGconn *conn = pg_get_connection();
    if (!conn) return -1;
    
    char room_id_str[16], user_id_str[16];
    snprintf(room_id_str, sizeof(room_id_str), "%d", room_id);
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    
    const char *query = "INSERT INTO room_users (room_id, user_id) VALUES ($1, $2);";
    const char *params[2] = {room_id_str, user_id_str};
    
    PGresult *res = PQexecParams(conn, query, 2, NULL, params, NULL, NULL, 0);
    int result = (PQresultStatus(res) == PGRES_COMMAND_OK) ? 0 : -1;
    
    PQclear(res);
    pg_return_connection(conn);
    return result;
}

int pg_remove_user_from_room(int room_id, int user_id) {
    PGconn *conn = pg_get_connection();
    if (!conn) return -1;
    
    char room_id_str[16], user_id_str[16];
    snprintf(room_id_str, sizeof(room_id_str), "%d", room_id);
    snprintf(user_id_str, sizeof(user_id_str), "%d", user_id);
    
    const char *query = "DELETE FROM room_users WHERE room_id = $1 AND user_id = $2;";
    const char *params[2] = {room_id_str, user_id_str};
    
    PGresult *res = PQexecParams(conn, query, 2, NULL, params, NULL, NULL, 0);
    int result = (PQresultStatus(res) == PGRES_COMMAND_OK) ? 0 : -1;
    
    PQclear(res);
    pg_return_connection(conn);
    return result;
}

// ============= MESSAGE OPERATIONS =============

int pg_create_message(int sender_id, int room_id, const char *sender_name, const char *content) {
    PGconn *conn = pg_get_connection();
    if (!conn) return -1;
    
    char sender_id_str[16], room_id_str[16];
    snprintf(sender_id_str, sizeof(sender_id_str), "%d", sender_id);
    snprintf(room_id_str, sizeof(room_id_str), "%d", room_id);
    
    const char *query = "INSERT INTO messages (sender_id, room_id, sender_name, content) VALUES ($1, $2, $3, $4) RETURNING message_id;";
    const char *params[4] = {sender_id_str, room_id_str, sender_name, content};
    
    PGresult *res = PQexecParams(conn, query, 4, NULL, params, NULL, NULL, 0);
    int message_id = -1;
    
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        message_id = atoi(PQgetvalue(res, 0, 0));
    }
    
    PQclear(res);
    pg_return_connection(conn);
    return message_id;
}

int pg_get_room_messages(int room_id, int limit) {
    PGconn *conn = pg_get_connection();
    if (!conn) return -1;
    
    char room_id_str[16], limit_str[16];
    snprintf(room_id_str, sizeof(room_id_str), "%d", room_id);
    snprintf(limit_str, sizeof(limit_str), "%d", limit);
    
    const char *query = "SELECT * FROM messages WHERE room_id = $1 ORDER BY timestamp DESC LIMIT $2;";
    const char *params[2] = {room_id_str, limit_str};
    
    PGresult *res = PQexecParams(conn, query, 2, NULL, params, NULL, NULL, 0);
    int count = PQntuples(res);
    
    PQclear(res);
    pg_return_connection(conn);
    return count;
}
