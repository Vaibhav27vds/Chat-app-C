#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdio.h>

typedef struct {
    void (*task)(void *);
    void *arg;
} WorkItem;

typedef struct {
    pthread_t *threads;
    int thread_count;
    
    // Queue for work items
    WorkItem *queue;
    int queue_size;
    int queue_front;
    int queue_rear;
    int queue_count;

    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_not_empty;

    int is_running;
    int is_shutdown;
} ThreadPool;
int thread_pool_init(int num_threads, int queue_size);

int thread_pool_submit(void (*task)(void *), void *arg);

void thread_pool_shutdown();

void thread_pool_cleanup();

int thread_pool_get_queue_size();

#endif 

