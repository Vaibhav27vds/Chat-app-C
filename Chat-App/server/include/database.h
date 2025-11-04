#ifndef DATABASE_H
#define DATABASE_H

#include <time.h>
#include <pthread.h>

#define MAX_USERS 1000
#define MAX_ROOMS 100
#define MAX_MESSAGES 10000
#define MAX_USERS_PER_ROOM 50

typedef enum {
    ROLE_USER = 0,
    ROLE_ADMIN = 1
} UserRole;


typedef struct {
    int user_id;
    char username[50];
    char password_hash[65];
    UserRole role;
    int current_room_id;
    time_t created_at;
    int is_active;
    int is_online;
} User;

typedef struct {
    int room_id;
    char room_name[100];
    int created_by;
    int max_users;
    int current_user_count;
    int user_ids[MAX_USERS_PER_ROOM];
    time_t created_at;
    int is_active;
} ChatRoom;

typedef struct {
    int message_id;
    int sender_id;
    int room_id;
    char sender_name[50];
    char content[1024];
    time_t timestamp;
} Message;

typedef struct {
    User users[MAX_USERS];
    int user_count;
    int next_user_id;
    
    ChatRoom rooms[MAX_ROOMS];
    int room_count;
    int next_room_id;
    
    Message messages[MAX_MESSAGES];
    int message_count;
    int next_message_id;
    
    pthread_mutex_t mutex_users;
    pthread_mutex_t mutex_rooms;
    pthread_mutex_t mutex_messages;
} Database;

extern Database g_db;

void db_init();
void db_cleanup();

int db_create_user(const char *username, const char *password_hash, UserRole role);
User* db_get_user_by_id(int user_id);
User* db_get_user_by_username(const char *username);
int db_user_exists(const char *username);
int db_update_user_online_status(int user_id, int is_online);

int db_create_room(const char *room_name, int created_by);
ChatRoom* db_get_room_by_id(int room_id);
int db_add_user_to_room(int room_id, int user_id);
int db_remove_user_from_room(int room_id, int user_id);
int db_get_room_users(int room_id, int *user_ids, int max_count);
ChatRoom* db_get_all_rooms(int *count);

int db_create_message(int sender_id, int room_id, const char *sender_name, const char *content);
Message* db_get_room_messages(int room_id, int *count, int limit);
Message* db_get_all_messages(int *count);

void db_print_stats();
void db_print_users();
void db_print_rooms();

#endif 