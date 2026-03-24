#ifndef LOGGER_PROC_H
#define LOGGER_PROC_H

/*
 * Logger process entry function.
 * This process reads one log line at a time from FIFO
 * and writes formatted lines into gateway.log.
 */
void logger_process_run(void);

#endif /* LOGGER_PROC_H */