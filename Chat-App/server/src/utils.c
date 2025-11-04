#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/socket.h>

// ============= STRING UTILITIES =============

void str_trim(char *str) {
    if (!str) return;
    
    char *start = str;
    char *end;
    
    // Find start of non-whitespace
    while (*start && isspace((unsigned char)*start)) start++;
    
    if (*start == 0) {
        str[0] = '\0';
        return;
    }
    
    // Find end of non-whitespace
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    
    // Copy trimmed string
    int len = end - start + 1;
    memmove(str, start, len);
    str[len] = '\0';
}

int str_starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) return 0;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

int str_ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;
    
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len) return 0;
    
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

void str_to_lower(char *str) {
    if (!str) return;
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

void str_to_upper(char *str) {
    if (!str) return;
    for (int i = 0; str[i]; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
}

// ============= JSON UTILITIES =============

void json_create_error(char *buffer, size_t size, const char *error_msg) {
    snprintf(buffer, size, "{\"status\": \"error\", \"message\": \"%s\"}", error_msg);
}

void json_create_success(char *buffer, size_t size, const char *data) {
    snprintf(buffer, size, "{\"status\": \"success\", \"data\": %s}", data);
}

void json_add_string(char *buffer, size_t size, const char *key, const char *value) {
    size_t len = strlen(buffer);
    snprintf(buffer + len, size - len, ",\"%s\": \"%s\"", key, value);
}

void json_add_int(char *buffer, size_t size, const char *key, int value) {
    size_t len = strlen(buffer);
    snprintf(buffer + len, size - len, ",\"%s\": %d", key, value);
}

// ============= TIME UTILITIES =============

time_t get_current_timestamp() {
    return time(NULL);
}

char* timestamp_to_string(time_t timestamp) {
    static char buffer[32];
    struct tm *tm_info = localtime(&timestamp);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

// ============= SOCKET UTILITIES =============

int socket_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int socket_set_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

int socket_set_reuseaddr(int fd) {
    int opt = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

// ============= LOGGING =============

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    
    printf("[INFO %s] ", time_str);
    vprintf(format, args);
    printf("\n");
    
    va_end(args);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    
    fprintf(stderr, "[ERROR %s] ", time_str);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    
    va_end(args);
}

void log_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    
    printf("[DEBUG %s] ", time_str);
    vprintf(format, args);
    printf("\n");
    
    va_end(args);
}
