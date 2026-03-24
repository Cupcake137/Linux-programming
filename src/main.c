#include "common.h"
#include "logging.h"
#include "logger_proc.h"
#include "thread_workers.h"
#include "sbuffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

/*
 * Signal handler:
 * - Only set g_running to 0
 * - Do not printf/log here
 */
static void on_sigint(int signo)
{
    (void)signo;
    atomic_store(&g_running, 0);
}

int main(int argc, char *argv[])
{
    /* Parse TCP server port from command line */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        return 1;
    }

    printf("Starting gateway on port %d\n", port);
    fflush(stdout);

    /* Create FIFO if it does not exist yet */
    if (access(FIFO_NAME, F_OK) == -1) {
        if (mkfifo(FIFO_NAME, 0666) != 0) {
            perror("mkfifo");
            return 1;
        }
    }

    /* Fork logger process */
    pid_t logger_pid = fork();
    if (logger_pid < 0) {
        perror("fork");
        return 1;
    }

    if (logger_pid == 0) {
        logger_process_run();
        _exit(0);
    }

    /* Install SIGINT handler (stop with: kill -INT <pid>) */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) != 0) {
        perror("sigaction(SIGINT)");
        return 1;
    }

    /* Initialize FIFO-based logging */
    log_init();
    log_event("Gateway started on port %d", port);

    /* Create shared buffer */
    sbuffer_t *buffer = sbuffer_create();
    if (buffer == NULL) {
        fprintf(stderr, "Failed to create sbuffer\n");
        return 1;
    }

    /* Shared context for all worker threads */
    gateway_ctx_t ctx;
    ctx.buffer = buffer;
    ctx.port = port;

    /* Create worker threads */
    pthread_t t_conn, t_data, t_store;

    if (pthread_create(&t_conn, NULL, connection_manager_thread, &ctx) != 0) {
        perror("pthread_create connection_manager_thread");
        return 1;
    }

    if (pthread_create(&t_data, NULL, data_manager_thread, &ctx) != 0) {
        perror("pthread_create data_manager_thread");
        return 1;
    }

    if (pthread_create(&t_store, NULL, storage_manager_thread, &ctx) != 0) {
        perror("pthread_create storage_manager_thread");
        return 1;
    }

    /*
     * Wait for connection manager to stop first.
     * In this project version, SIGINT sets g_running=0,
     * connection_manager exits, then we close the sbuffer
     * so consumer threads can wake up and exit cleanly.
     */
    pthread_join(t_conn, NULL);

    sbuffer_close(buffer);

    pthread_join(t_data, NULL);
    pthread_join(t_store, NULL);

    sbuffer_destroy(buffer);

    /* Final shutdown */
    log_event("Gateway shutting down");
    log_event("SHUTDOWN");
    log_close();

    waitpid(logger_pid, NULL, 0);

    return 0;
}