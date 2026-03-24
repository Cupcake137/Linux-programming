#include "logging.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

/* FIFO writer file descriptor used by main process */
static int g_fifo_fd = -1;

/* Protect concurrent log_event() calls from multiple threads */
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_init(void)
{
    /*
     * Open FIFO for writing.
     * This blocks until logger process opens FIFO for reading.
     */
    g_fifo_fd = open(FIFO_NAME, O_WRONLY);
    if (g_fifo_fd < 0) {
        perror("log_init: open FIFO for write");
        exit(EXIT_FAILURE);
    }
}

void log_event(const char *fmt, ...)
{
    pthread_mutex_lock(&g_log_mutex);

    char buf[512];

    /* Build message using printf-style formatting */
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    /*
     * Ensure every log event ends with '\n'
     * so logger process can read line-by-line.
     */
    size_t len = strlen(buf);
    if (len == 0 || buf[len - 1] != '\n') {
        if (len < sizeof(buf) - 1) {
            buf[len] = '\n';
            buf[len + 1] = '\0';
        }
    }

    (void)write(g_fifo_fd, buf, strlen(buf));

    pthread_mutex_unlock(&g_log_mutex);
}

void log_close(void)
{
    if (g_fifo_fd >= 0) {
        close(g_fifo_fd);
        g_fifo_fd = -1;
    }
}