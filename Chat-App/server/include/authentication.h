#ifndef AUTHENTICATION_H
#define AUTHENTICATION_H

#include "database.h"

void auth_hash_password(const char *password, char *hash_out);

int auth_verify_password(const char *password, const char *hash);

int auth_generate_token(int user_id, char *token_out, size_t token_size);

int auth_validate_token(const char *token, int *user_id_out);

int auth_login(const char *username, const char *password, int *user_id_out);

int auth_register(const char *username, const char *password, UserRole role, int *user_id_out);

#endif 
