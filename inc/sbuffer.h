#ifndef SBUFFER_H
#define SBUFFER_H

#include <stdint.h>

/*
 * Simple sensor data structure used from milestone 3 onward.
 * This is enough for:
 * - TCP receiving
 * - shared buffer
 * - data/storage consuming
 */
typedef struct {
    uint16_t sensor_id;
    double value;
    int64_t ts;
} sensor_data_t;

/* Identify which consumer is reading from sbuffer */
typedef enum {
    CONSUMER_DATA_MANAGER = 0,
    CONSUMER_STORAGE_MANAGER = 1
} consumer_t;

/* Opaque buffer type */
typedef struct sbuffer sbuffer_t;

/* Create / destroy buffer */
sbuffer_t *sbuffer_create(void);
void sbuffer_destroy(sbuffer_t *b);

/* Push one item into buffer (producer side) */
int sbuffer_push(sbuffer_t *b, const sensor_data_t *data);

/*
 * Read one item for a specific consumer.
 *
 * Return values:
 *   0  -> success
 *   1  -> buffer closed and empty, caller should exit
 *  -1  -> error
 */
int sbuffer_pop_for(sbuffer_t *b, consumer_t who, sensor_data_t *out);

/* Close buffer so waiting consumers can wake up and exit */
void sbuffer_close(sbuffer_t *b);

#endif /* SBUFFER_H */