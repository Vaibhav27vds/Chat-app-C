#include "thread_pool.h"
#include <stdlib.h>
#include <string.h>

static ThreadPool g_thread_pool = {0};

// Worker thread function
static void* worker_thread(void *arg) {
    while (1) {
        pthread_mutex_lock(&g_thread_pool.queue_mutex);

        while (g_thread_pool.queue_count == 0 && !g_thread_pool.is_shutdown) {
            pthread_cond_wait(&g_thread_pool.queue_not_empty, &g_thread_pool.queue_mutex);
        }
        

        if (g_thread_pool.is_shutdown && g_thread_pool.queue_count == 0) {
            pthread_mutex_unlock(&g_thread_pool.queue_mutex);
            break;
        }
        
        // Get work item from queue
        WorkItem work = g_thread_pool.queue[g_thread_pool.queue_front];
        g_thread_pool.queue_front = (g_thread_pool.queue_front + 1) % g_thread_pool.queue_size;
        g_thread_pool.queue_count--;
        
        pthread_mutex_unlock(&g_thread_pool.queue_mutex);
        
        // Execute work
        if (work.task) {
            work.task(work.arg);
        }
    }
    
    return NULL;
}

int thread_pool_init(int num_threads, int queue_size) {
    memset(&g_thread_pool, 0, sizeof(ThreadPool));
    
    g_thread_pool.thread_count = num_threads;
    g_thread_pool.queue_size = queue_size;
    g_thread_pool.is_running = 1;
    
    // Allocate threads
    g_thread_pool.threads = malloc(sizeof(pthread_t) * num_threads);
    if (!g_thread_pool.threads) {
        return -1;
    }
    
    // Allocate queue
    g_thread_pool.queue = malloc(sizeof(WorkItem) * queue_size);
    if (!g_thread_pool.queue) {
        free(g_thread_pool.threads);
        return -1;
    }
    
    // Initialize synchronization primitives
    pthread_mutex_init(&g_thread_pool.queue_mutex, NULL);
    pthread_cond_init(&g_thread_pool.queue_not_empty, NULL);
    
    // Create worker threads
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&g_thread_pool.threads[i], NULL, worker_thread, NULL) != 0) {
            fprintf(stderr, "Error creating thread %d\n", i);
            return -1;
        }
    }
    
    printf("[THREADPOOL] Initialized with %d threads and queue size %d\n", num_threads, queue_size);
    
    return 0;
}

int thread_pool_submit(void (*task)(void *), void *arg) {
    pthread_mutex_lock(&g_thread_pool.queue_mutex);

    if (g_thread_pool.queue_count >= g_thread_pool.queue_size) {
        pthread_mutex_unlock(&g_thread_pool.queue_mutex);
        return -1;  
    }

    int rear = (g_thread_pool.queue_front + g_thread_pool.queue_count) % g_thread_pool.queue_size;
    g_thread_pool.queue[rear].task = task;
    g_thread_pool.queue[rear].arg = arg;
    g_thread_pool.queue_count++;
    
    pthread_cond_signal(&g_thread_pool.queue_not_empty);
    
    pthread_mutex_unlock(&g_thread_pool.queue_mutex);
    
    return 0;
}

void thread_pool_shutdown() {
    pthread_mutex_lock(&g_thread_pool.queue_mutex);
    g_thread_pool.is_shutdown = 1;
    pthread_cond_broadcast(&g_thread_pool.queue_not_empty);
    pthread_mutex_unlock(&g_thread_pool.queue_mutex);
    
    for (int i = 0; i < g_thread_pool.thread_count; i++) {
        pthread_join(g_thread_pool.threads[i], NULL);
    }
    
    printf("[THREADPOOL] Shutdown complete\n");
}

void thread_pool_cleanup() {
    if (g_thread_pool.threads) {
        free(g_thread_pool.threads);
    }
    if (g_thread_pool.queue) {
        free(g_thread_pool.queue);
    }
    
    pthread_mutex_destroy(&g_thread_pool.queue_mutex);
    pthread_cond_destroy(&g_thread_pool.queue_not_empty);
    
    printf("[THREADPOOL] Cleanup complete\n");
}

int thread_pool_get_queue_size() {
    pthread_mutex_lock(&g_thread_pool.queue_mutex);
    int size = g_thread_pool.queue_count;
    pthread_mutex_unlock(&g_thread_pool.queue_mutex);
    return size;
}
