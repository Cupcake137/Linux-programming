#include <stdio.h>
#include <unistd.h>
#include <stdatomic.h>
#include "thread_workers.h"
#include "logging.h"

atomic_int g_running = 1;

void *connection_manager(void *arg) {
    (void)arg;
    log_event("connection_manager: started");
    while (atomic_load(&g_running)) {
        // giả lập làm việc
        sleep(1);
    }
    log_event("connection_manager: exiting");
    return NULL;
}

void *data_manager(void *arg) {
    (void)arg;
    log_event("data_manager: started");
    while (atomic_load(&g_running)) {
        sleep(1);
    }
    log_event("data_manager: exiting");
    return NULL;
}

void *storage_manager(void *arg) {
    (void)arg;
    log_event("storage_manager: started");
    while (atomic_load(&g_running)) {
        sleep(1);
    }
    log_event("storage_manager: exiting");
    return NULL;
}