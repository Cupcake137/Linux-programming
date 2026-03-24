#include "logger_proc.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

void logger_process_run(void)
{
    /*
     * Open FIFO for reading.
     * This blocks until the main process opens FIFO for writing.
     */
    int fd = open(FIFO_NAME, O_RDONLY);
    if (fd < 0) {
        perror("logger: open FIFO for read");
        exit(EXIT_FAILURE);
    }

    FILE *fifo_fp = fdopen(fd, "r");
    if (fifo_fp == NULL) {
        perror("logger: fdopen");
        exit(EXIT_FAILURE);
    }

    FILE *log_fp = fopen(LOG_FILE, "w");
    if (log_fp == NULL) {
        perror("logger: fopen log file");
        exit(EXIT_FAILURE);
    }

    char line[512];
    int seq = 0;

    while (fgets(line, sizeof(line), fifo_fp) != NULL) {
        /* Remove trailing newline characters */
        line[strcspn(line, "\r\n")] = '\0';

        /* Main process uses this command to stop the logger cleanly */
        if (strcmp(line, "SHUTDOWN") == 0) {
            break;
        }

        /* Build human-readable local timestamp */
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);

        char tbuf[32];
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm_now);

        /*
         * Final log format:
         *   <sequence> <YYYY-MM-DD HH:MM:SS> <message>
         */
        fprintf(log_fp, "%d %s %s\n", ++seq, tbuf, line);
        fflush(log_fp);
    }

    fclose(log_fp);
    fclose(fifo_fp);
}