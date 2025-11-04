#ifndef UTILS_H
#define UTILS_H

#include <time.h>

// String utilities
void str_trim(char *str);
int str_starts_with(const char *str, const char *prefix);
int str_ends_with(const char *str, const char *suffix);
void str_to_lower(char *str);
void str_to_upper(char *str);

// JSON utilities
void json_create_error(char *buffer, size_t size, const char *error_msg);
void json_create_success(char *buffer, size_t size, const char *data);
void json_add_string(char *buffer, size_t size, const char *key, const char *value);
void json_add_int(char *buffer, size_t size, const char *key, int value);

// Time utilities
time_t get_current_timestamp();
char* timestamp_to_string(time_t timestamp);

// Socket utilities
int socket_set_nonblocking(int fd);
int socket_set_blocking(int fd);
int socket_set_reuseaddr(int fd);

// Logging
void log_info(const char *format, ...);
void log_error(const char *format, ...);
void log_debug(const char *format, ...);

#endif // UTILS_H
