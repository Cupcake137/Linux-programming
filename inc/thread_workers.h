#ifndef THREAD_WORKERS_H
#define THREAD_WORKERS_H

#include <stdatomic.h>
#include "sbuffer.h"

/*
 * Global run flag:
 * - defined exactly once in thread_workers.c
 * - set to 0 when program should stop
 */
extern atomic_int g_running;

/*
 * Shared context passed to all worker threads.
 */
typedef struct {
    sbuffer_t *buffer;
    int port;
} gateway_ctx_t;

void *connection_manager_thread(void *arg);
void *data_manager_thread(void *arg);
void *storage_manager_thread(void *arg);

#endif /* THREAD_WORKERS_H */