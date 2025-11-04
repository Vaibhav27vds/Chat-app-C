#ifndef POSTGRES_DB_H
#define POSTGRES_DB_H

#include <postgresql/libpq-fe.h>
#include <time.h>
#include <pthread.h>

// Connection pool structure
typedef struct {
    PGconn *conn;
    int in_use;
} PooledConnection;

typedef struct {
    PooledConnection *connections;
    int pool_size;
    pthread_mutex_t mutex;
} ConnectionPool;

// Database connection functions
int pg_init_connection_pool(const char *host, const char *port, 
                            const char *database, const char *user, 
                            const char *password, int pool_size);

PGconn* pg_get_connection();
void pg_return_connection(PGconn *conn);
void pg_cleanup_connection_pool();

// User operations
int pg_create_user(const char *username, const char *password_hash, int role);
int pg_get_user_by_username(const char *username, int *user_id, int *role);
int pg_get_user_by_id(int user_id, char *username, int *role, int *is_online);
int pg_update_user_online_status(int user_id, int is_online);
int pg_user_exists(const char *username);

// Chat room operations
int pg_create_room(const char *room_name, int created_by);
int pg_get_all_rooms(void);
int pg_add_user_to_room(int room_id, int user_id);
int pg_remove_user_from_room(int room_id, int user_id);
int pg_get_room_users(int room_id);

// Message operations
int pg_create_message(int sender_id, int room_id, const char *sender_name, const char *content);
int pg_get_room_messages(int room_id, int limit);

#endif // POSTGRES_DB_H
