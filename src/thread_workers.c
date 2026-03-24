#include "thread_workers.h"
#include "logging.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Global run flag defined exactly once here */
atomic_int g_running = 1;

/*
 * connection_manager_thread:
 * - Creates a TCP server
 * - Accepts one client at a time
 * - Reads text lines:
 *      <sensor_id> <value> <timestamp>
 * - Pushes parsed measurements into sbuffer
 */
void *connection_manager_thread(void *arg)
{
    gateway_ctx_t *ctx = (gateway_ctx_t *)arg;
    log_event("connection_manager: started");

    int server_fd = -1;
    int client_fd = -1;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_event("connection_manager: socket() failed");
        log_event("connection_manager: exiting");
        return NULL;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(ctx->port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        log_event("connection_manager: bind() failed on port %d", ctx->port);
        close(server_fd);
        log_event("connection_manager: exiting");
        return NULL;
    }

    if (listen(server_fd, 5) < 0) {
        log_event("connection_manager: listen() failed");
        close(server_fd);
        log_event("connection_manager: exiting");
        return NULL;
    }

    log_event("connection_manager: listening on port %d", ctx->port);

    while (atomic_load(&g_running) != 0) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (atomic_load(&g_running) == 0) {
                break;
            }
            log_event("connection_manager: accept() failed");
            continue;
        }

        log_event("connection_manager: client connected from %s:%d",
                  inet_ntoa(client_addr.sin_addr),
                  ntohs(client_addr.sin_port));

        FILE *client_fp = fdopen(client_fd, "r");
        if (client_fp == NULL) {
            log_event("connection_manager: fdopen() failed");
            close(client_fd);
            client_fd = -1;
            continue;
        }

        char line[256];

        while (atomic_load(&g_running) != 0 &&
               fgets(line, sizeof(line), client_fp) != NULL) {

            sensor_data_t d;

            int matched = sscanf(line, "%hu %lf %lld",
                                 &d.sensor_id,
                                 &d.value,
                                 (long long *)&d.ts);

            if (matched == 3) {
                sbuffer_push(ctx->buffer, &d);
                log_event("connection_manager: received sensor=%u value=%.2f ts=%lld",
                          d.sensor_id,
                          d.value,
                          (long long)d.ts);
            } else {
                log_event("connection_manager: invalid input line: %s", line);
            }
        }

        log_event("connection_manager: client disconnected");
        fclose(client_fp); /* also closes client_fd */
        client_fd = -1;
    }

    if (client_fd >= 0) {
        close(client_fd);
    }
    if (server_fd >= 0) {
        close(server_fd);
    }

    log_event("connection_manager: exiting");
    return NULL;
}

/*
 * data_manager_thread:
 * For milestone 4, it only demonstrates that it can consume data
 * from sbuffer. Real data processing logic will be added later.
 */
void *data_manager_thread(void *arg)
{
    gateway_ctx_t *ctx = (gateway_ctx_t *)arg;
    log_event("data_manager: started");

    while (1) {
        sensor_data_t d;
        int rc = sbuffer_pop_for(ctx->buffer, CONSUMER_DATA_MANAGER, &d);

        if (rc == 1) {
            break;
        }
        if (rc != 0) {
            continue;
        }

        log_event("data_manager: consumed sensor=%u value=%.2f",
                  d.sensor_id, d.value);
    }

    log_event("data_manager: exiting");
    return NULL;
}

/*
 * storage_manager_thread:
 * For milestone 4, it also only demonstrates consumption.
 * Real DB storage will be added in a later milestone.
 */
void *storage_manager_thread(void *arg)
{
    gateway_ctx_t *ctx = (gateway_ctx_t *)arg;
    log_event("storage_manager: started");

    while (1) {
        sensor_data_t d;
        int rc = sbuffer_pop_for(ctx->buffer, CONSUMER_STORAGE_MANAGER, &d);

        if (rc == 1) {
            break;
        }
        if (rc != 0) {
            continue;
        }

        log_event("storage_manager: consumed sensor=%u value=%.2f",
                  d.sensor_id, d.value);
    }

    log_event("storage_manager: exiting");
    return NULL;
}