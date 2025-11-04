#include "database.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Global database instance
Database g_db;

void db_init() {
    memset(&g_db, 0, sizeof(Database));
    
    // Initialize mutexes
    pthread_mutex_init(&g_db.mutex_users, NULL);
    pthread_mutex_init(&g_db.mutex_rooms, NULL);
    pthread_mutex_init(&g_db.mutex_messages, NULL);
    
    g_db.next_user_id = 1;
    g_db.next_room_id = 1;
    g_db.next_message_id = 1;
    
    printf("[DB] Database initialized\n");
}

void db_cleanup() {
    pthread_mutex_destroy(&g_db.mutex_users);
    pthread_mutex_destroy(&g_db.mutex_rooms);
    pthread_mutex_destroy(&g_db.mutex_messages);
    
    printf("[DB] Database cleaned up\n");
}

int db_create_user(const char *username, const char *password_hash, UserRole role) {
    pthread_mutex_lock(&g_db.mutex_users);
    
    if (g_db.user_count >= MAX_USERS) {
        pthread_mutex_unlock(&g_db.mutex_users);
        return -1; 
    }

    for (int i = 0; i < g_db.user_count; i++) {
        if (strcmp(g_db.users[i].username, username) == 0) {
            pthread_mutex_unlock(&g_db.mutex_users);
            return -2; 
        }
    }
    User *new_user = &g_db.users[g_db.user_count];
    new_user->user_id = g_db.next_user_id++;
    strncpy(new_user->username, username, sizeof(new_user->username) - 1);
    strncpy(new_user->password_hash, password_hash, sizeof(new_user->password_hash) - 1);
    new_user->role = role;
    new_user->current_room_id = -1;
    new_user->created_at = time(NULL);
    new_user->is_active = 1;
    new_user->is_online = 0;
    
    int user_id = new_user->user_id;
    g_db.user_count++;
    
    pthread_mutex_unlock(&g_db.mutex_users);
    
    printf("[DB] User created: %s (ID: %d, Role: %s)\n", 
           username, user_id, role == ROLE_ADMIN ? "ADMIN" : "USER");
    
    return user_id;
}

User* db_get_user_by_id(int user_id) {
    pthread_mutex_lock(&g_db.mutex_users);
    
    for (int i = 0; i < g_db.user_count; i++) {
        if (g_db.users[i].user_id == user_id) {
            pthread_mutex_unlock(&g_db.mutex_users);
            return &g_db.users[i];
        }
    }
    
    pthread_mutex_unlock(&g_db.mutex_users);
    return NULL;
}

User* db_get_user_by_username(const char *username) {
    pthread_mutex_lock(&g_db.mutex_users);
    
    for (int i = 0; i < g_db.user_count; i++) {
        if (strcmp(g_db.users[i].username, username) == 0) {
            pthread_mutex_unlock(&g_db.mutex_users);
            return &g_db.users[i];
        }
    }
    
    pthread_mutex_unlock(&g_db.mutex_users);
    return NULL;
}

int db_user_exists(const char *username) {
    pthread_mutex_lock(&g_db.mutex_users);
    
    for (int i = 0; i < g_db.user_count; i++) {
        if (strcmp(g_db.users[i].username, username) == 0) {
            pthread_mutex_unlock(&g_db.mutex_users);
            return 1;
        }
    }
    
    pthread_mutex_unlock(&g_db.mutex_users);
    return 0;
}

int db_update_user_online_status(int user_id, int is_online) {
    pthread_mutex_lock(&g_db.mutex_users);
    
    for (int i = 0; i < g_db.user_count; i++) {
        if (g_db.users[i].user_id == user_id) {
            g_db.users[i].is_online = is_online;
            pthread_mutex_unlock(&g_db.mutex_users);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&g_db.mutex_users);
    return -1;
}

int db_create_room(const char *room_name, int created_by) {
    pthread_mutex_lock(&g_db.mutex_rooms);
    
    if (g_db.room_count >= MAX_ROOMS) {
        pthread_mutex_unlock(&g_db.mutex_rooms);
        return -1;
    }
    
    ChatRoom *new_room = &g_db.rooms[g_db.room_count];
    new_room->room_id = g_db.next_room_id++;
    strncpy(new_room->room_name, room_name, sizeof(new_room->room_name) - 1);
    new_room->created_by = created_by;
    new_room->max_users = MAX_USERS_PER_ROOM;
    new_room->current_user_count = 0;
    new_room->created_at = time(NULL);
    new_room->is_active = 1;
    
    int room_id = new_room->room_id;
    g_db.room_count++;
    
    pthread_mutex_unlock(&g_db.mutex_rooms);
    
    printf("[DB] Room created: %s (ID: %d, Creator: %d)\n", room_name, room_id, created_by);
    
    return room_id;
}

ChatRoom* db_get_room_by_id(int room_id) {
    pthread_mutex_lock(&g_db.mutex_rooms);
    
    for (int i = 0; i < g_db.room_count; i++) {
        if (g_db.rooms[i].room_id == room_id) {
            pthread_mutex_unlock(&g_db.mutex_rooms);
            return &g_db.rooms[i];
        }
    }
    
    pthread_mutex_unlock(&g_db.mutex_rooms);
    return NULL;
}

int db_add_user_to_room(int room_id, int user_id) {
    pthread_mutex_lock(&g_db.mutex_rooms);
    
    for (int i = 0; i < g_db.room_count; i++) {
        if (g_db.rooms[i].room_id == room_id) {
            if (g_db.rooms[i].current_user_count >= g_db.rooms[i].max_users) {
                pthread_mutex_unlock(&g_db.mutex_rooms);
                return -1;  // Room full
            }
            
            // Check if user already in room
            for (int j = 0; j < g_db.rooms[i].current_user_count; j++) {
                if (g_db.rooms[i].user_ids[j] == user_id) {
                    pthread_mutex_unlock(&g_db.mutex_rooms);
                    return -2;  // User already in room
                }
            }
            
            g_db.rooms[i].user_ids[g_db.rooms[i].current_user_count++] = user_id;
            
            pthread_mutex_unlock(&g_db.mutex_rooms);
            printf("[DB] User %d added to room %d\n", user_id, room_id);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&g_db.mutex_rooms);
    return -3;  // Room not found
}

int db_remove_user_from_room(int room_id, int user_id) {
    pthread_mutex_lock(&g_db.mutex_rooms);
    
    for (int i = 0; i < g_db.room_count; i++) {
        if (g_db.rooms[i].room_id == room_id) {
            for (int j = 0; j < g_db.rooms[i].current_user_count; j++) {
                if (g_db.rooms[i].user_ids[j] == user_id) {
                    // Shift remaining users
                    for (int k = j; k < g_db.rooms[i].current_user_count - 1; k++) {
                        g_db.rooms[i].user_ids[k] = g_db.rooms[i].user_ids[k + 1];
                    }
                    g_db.rooms[i].current_user_count--;
                    
                    pthread_mutex_unlock(&g_db.mutex_rooms);
                    printf("[DB] User %d removed from room %d\n", user_id, room_id);
                    return 0;
                }
            }
        }
    }
    
    pthread_mutex_unlock(&g_db.mutex_rooms);
    return -1;
}

int db_get_room_users(int room_id, int *user_ids, int max_count) {
    pthread_mutex_lock(&g_db.mutex_rooms);
    
    for (int i = 0; i < g_db.room_count; i++) {
        if (g_db.rooms[i].room_id == room_id) {
            int count = g_db.rooms[i].current_user_count;
            if (count > max_count) count = max_count;
            
            for (int j = 0; j < count; j++) {
                user_ids[j] = g_db.rooms[i].user_ids[j];
            }
            
            pthread_mutex_unlock(&g_db.mutex_rooms);
            return count;
        }
    }
    
    pthread_mutex_unlock(&g_db.mutex_rooms);
    return 0;
}

ChatRoom* db_get_all_rooms(int *count) {
    pthread_mutex_lock(&g_db.mutex_rooms);
    
    *count = g_db.room_count;
    ChatRoom *rooms = malloc(sizeof(ChatRoom) * g_db.room_count);
    
    if (rooms) {
        memcpy(rooms, g_db.rooms, sizeof(ChatRoom) * g_db.room_count);
    }
    
    pthread_mutex_unlock(&g_db.mutex_rooms);
    return rooms;
}

// ============= MESSAGE OPERATIONS =============

int db_create_message(int sender_id, int room_id, const char *sender_name, const char *content) {
    pthread_mutex_lock(&g_db.mutex_messages);
    
    if (g_db.message_count >= MAX_MESSAGES) {
        pthread_mutex_unlock(&g_db.mutex_messages);
        return -1;
    }
    
    Message *msg = &g_db.messages[g_db.message_count];
    msg->message_id = g_db.next_message_id++;
    msg->sender_id = sender_id;
    msg->room_id = room_id;
    strncpy(msg->sender_name, sender_name, sizeof(msg->sender_name) - 1);
    strncpy(msg->content, content, sizeof(msg->content) - 1);
    msg->timestamp = time(NULL);
    
    int message_id = msg->message_id;
    g_db.message_count++;
    
    pthread_mutex_unlock(&g_db.mutex_messages);
    
    printf("[DB] Message created (ID: %d, Sender: %s, Room: %d)\n", message_id, sender_name, room_id);
    
    return message_id;
}

Message* db_get_room_messages(int room_id, int *count, int limit) {
    pthread_mutex_lock(&g_db.mutex_messages);
    
    int msg_count = 0;
    int start_idx = 0;
    
    // Count messages in room
    for (int i = 0; i < g_db.message_count; i++) {
        if (g_db.messages[i].room_id == room_id) {
            msg_count++;
        }
    }
    
    if (limit > 0 && msg_count > limit) {
        start_idx = msg_count - limit;
        msg_count = limit;
    }
    
    Message *result = malloc(sizeof(Message) * msg_count);
    if (!result) {
        pthread_mutex_unlock(&g_db.mutex_messages);
        *count = 0;
        return NULL;
    }
    
    int result_idx = 0;
    int skipped = 0;
    
    for (int i = 0; i < g_db.message_count; i++) {
        if (g_db.messages[i].room_id == room_id) {
            if (skipped >= start_idx) {
                result[result_idx++] = g_db.messages[i];
            }
            skipped++;
        }
    }
    
    pthread_mutex_unlock(&g_db.mutex_messages);
    
    *count = msg_count;
    return result;
}

Message* db_get_all_messages(int *count) {
    pthread_mutex_lock(&g_db.mutex_messages);
    
    *count = g_db.message_count;
    Message *messages = malloc(sizeof(Message) * g_db.message_count);
    
    if (messages) {
        memcpy(messages, g_db.messages, sizeof(Message) * g_db.message_count);
    }
    
    pthread_mutex_unlock(&g_db.mutex_messages);
    return messages;
}

// ============= UTILITY FUNCTIONS =============

void db_print_stats() {
    pthread_mutex_lock(&g_db.mutex_users);
    pthread_mutex_lock(&g_db.mutex_rooms);
    pthread_mutex_lock(&g_db.mutex_messages);
    
    printf("\n========== DATABASE STATISTICS ==========\n");
    printf("Users: %d/%d\n", g_db.user_count, MAX_USERS);
    printf("Rooms: %d/%d\n", g_db.room_count, MAX_ROOMS);
    printf("Messages: %d/%d\n", g_db.message_count, MAX_MESSAGES);
    printf("=========================================\n\n");
    
    pthread_mutex_unlock(&g_db.mutex_messages);
    pthread_mutex_unlock(&g_db.mutex_rooms);
    pthread_mutex_unlock(&g_db.mutex_users);
}

void db_print_users() {
    pthread_mutex_lock(&g_db.mutex_users);
    
    printf("\n========== USERS ==========\n");
    for (int i = 0; i < g_db.user_count; i++) {
        printf("ID: %d | Username: %s | Role: %s | Online: %d | Room: %d\n",
               g_db.users[i].user_id,
               g_db.users[i].username,
               g_db.users[i].role == ROLE_ADMIN ? "ADMIN" : "USER",
               g_db.users[i].is_online,
               g_db.users[i].current_room_id);
    }
    printf("===========================\n\n");
    
    pthread_mutex_unlock(&g_db.mutex_users);
}

void db_print_rooms() {
    pthread_mutex_lock(&g_db.mutex_rooms);
    
    printf("\n========== ROOMS ==========\n");
    for (int i = 0; i < g_db.room_count; i++) {
        printf("ID: %d | Name: %s | Users: %d/%d | Created by: %d\n",
               g_db.rooms[i].room_id,
               g_db.rooms[i].room_name,
               g_db.rooms[i].current_user_count,
               g_db.rooms[i].max_users,
               g_db.rooms[i].created_by);
    }
    printf("===========================\n\n");
    
    pthread_mutex_unlock(&g_db.mutex_rooms);
}
