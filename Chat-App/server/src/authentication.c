#include "authentication.h"
#include "utils.h"
#include "database.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void auth_hash_password(const char *password, char *hash_out) {

    unsigned int hash = 5381;
    int c;
    
    const char* str = password;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }

    unsigned int hash2 = hash;
    int pwd_len = strlen(password);
    for (int i = 0; i < pwd_len; i++) {
        hash2 = (hash2 * 17 + password[i]) ^ 0xDEADBEEF;
    }
    
    // Convert to hex string (64 characters for SHA256-like output)
    sprintf(hash_out, "%08x%08x%08x%08x%08x%08x%08x%08x", 
            hash, hash2, hash ^ 0x12345678, hash2 ^ 0x87654321,
            hash * 7, hash2 * 11, hash ^ hash2, hash2 + hash);
    
    hash_out[64] = '\0';
}

int auth_verify_password(const char *password, const char *hash) {
    char computed_hash[65];
    auth_hash_password(password, computed_hash);
    
    return strcmp(computed_hash, hash) == 0 ? 1 : 0;
}
int auth_generate_token(int user_id, char *token_out, size_t token_size) {
    time_t now = time(NULL);
    snprintf(token_out, token_size, "%d.%ld", user_id, now);
    return 0;
}

int auth_validate_token(const char *token, int *user_id_out) {
    int user_id;
    long timestamp;
    
    if (sscanf(token, "%d.%ld", &user_id, &timestamp) != 2) {
        return -1;
    }

    
    time_t now = time(NULL);
    if (now - timestamp > 86400) {
        return -2;  // Token expired
    }
    
    *user_id_out = user_id;
    return 0;
}

// User login
int auth_login(const char *username, const char *password, int *user_id_out) {

    User *user = db_get_user_by_username(username);
    if (!user) {
        log_error("Login failed: user %s not found", username);
        return -1;
    }

    if (!auth_verify_password(password, user->password_hash)) {
        log_error("Login failed: invalid password for user %s", username);
        return -2;
    }
    
    // Update user online status
    user->is_online = 1;
    
    *user_id_out = user->user_id;
    log_info("User logged in: %s (ID: %d)", username, user->user_id);
    
    return 0;
}

int auth_register(const char *username, const char *password, UserRole role, int *user_id_out) {

    if (strlen(username) < 3 || strlen(username) > 49) {
        log_error("Registration failed: username length invalid");
        return -1;
    }
    
    if (strlen(password) < 6 || strlen(password) > 63) {
        log_error("Registration failed: password length invalid");
        return -2;
    }

    if (db_user_exists(username)) {
        log_error("Registration failed: user %s already exists", username);
        return -3;
    }

    char password_hash[65];
    auth_hash_password(password, password_hash);
    
    int user_id = db_create_user(username, password_hash, role);
    
    if (user_id < 0) {
        log_error("Registration failed: database error");
        return -4;
    }
    
    *user_id_out = user_id;
    log_info("User registered: %s (ID: %d, Role: %s)", 
             username, user_id, role == ROLE_ADMIN ? "ADMIN" : "USER");
    
    return 0;
}
