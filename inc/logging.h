#ifndef LOGGING_H
#define LOGGING_H

/*
 * Logging module used by the main process and its worker threads.
 *
 * Flow:
 *   log_event(...) -> FIFO -> logger process -> gateway.log
 */

void log_init(void);
void log_event(const char *fmt, ...);
void log_close(void);

#endif /* LOGGING_H */