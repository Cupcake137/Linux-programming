#include "sbuffer.h"

#include <stdlib.h>
#include <pthread.h>

/*
 * Each node must be consumed by TWO consumers:
 * - data manager
 * - storage manager
 *
 * The node is removed only after both have read it.
 */
typedef struct sbuffer_node {
    sensor_data_t data;
    int read_by_data_mgr;
    int read_by_storage_mgr;
    struct sbuffer_node *next;
} sbuffer_node_t;

struct sbuffer {
    sbuffer_node_t *head;
    sbuffer_node_t *tail;

    int closed;

    pthread_mutex_t mtx;
    pthread_cond_t cond;
};

/* Remove fully consumed nodes from the head */
static void remove_fully_consumed_head(sbuffer_t *b)
{
    while (b->head != NULL &&
           b->head->read_by_data_mgr &&
           b->head->read_by_storage_mgr) {

        sbuffer_node_t *old = b->head;
        b->head = old->next;

        if (b->head == NULL) {
            b->tail = NULL;
        }

        free(old);
    }
}

sbuffer_t *sbuffer_create(void)
{
    sbuffer_t *b = (sbuffer_t *)calloc(1, sizeof(sbuffer_t));
    if (b == NULL) {
        return NULL;
    }

    pthread_mutex_init(&b->mtx, NULL);
    pthread_cond_init(&b->cond, NULL);

    b->head = NULL;
    b->tail = NULL;
    b->closed = 0;

    return b;
}

void sbuffer_destroy(sbuffer_t *b)
{
    if (b == NULL) {
        return;
    }

    pthread_mutex_lock(&b->mtx);

    sbuffer_node_t *cur = b->head;
    while (cur != NULL) {
        sbuffer_node_t *next = cur->next;
        free(cur);
        cur = next;
    }

    b->head = NULL;
    b->tail = NULL;

    pthread_mutex_unlock(&b->mtx);

    pthread_mutex_destroy(&b->mtx);
    pthread_cond_destroy(&b->cond);

    free(b);
}

int sbuffer_push(sbuffer_t *b, const sensor_data_t *data)
{
    if (b == NULL || data == NULL) {
        return -1;
    }

    sbuffer_node_t *node = (sbuffer_node_t *)calloc(1, sizeof(sbuffer_node_t));
    if (node == NULL) {
        return -1;
    }

    node->data = *data;
    node->read_by_data_mgr = 0;
    node->read_by_storage_mgr = 0;
    node->next = NULL;

    pthread_mutex_lock(&b->mtx);

    if (b->closed) {
        pthread_mutex_unlock(&b->mtx);
        free(node);
        return -1;
    }

    if (b->tail != NULL) {
        b->tail->next = node;
        b->tail = node;
    } else {
        b->head = node;
        b->tail = node;
    }

    pthread_cond_broadcast(&b->cond);
    pthread_mutex_unlock(&b->mtx);

    return 0;
}

int sbuffer_pop_for(sbuffer_t *b, consumer_t who, sensor_data_t *out)
{
    if (b == NULL || out == NULL) {
        return -1;
    }

    pthread_mutex_lock(&b->mtx);

    while (1) {
        /* Clean up any nodes already consumed by both threads */
        remove_fully_consumed_head(b);

        /* If buffer empty and closed => consumer can exit */
        if (b->head == NULL && b->closed) {
            pthread_mutex_unlock(&b->mtx);
            return 1;
        }

        /* If buffer empty but still open => wait for new data */
        if (b->head == NULL) {
            pthread_cond_wait(&b->cond, &b->mtx);
            continue;
        }

        sbuffer_node_t *node = b->head;

        if (who == CONSUMER_DATA_MANAGER) {
            if (!node->read_by_data_mgr) {
                *out = node->data;
                node->read_by_data_mgr = 1;

                remove_fully_consumed_head(b);
                pthread_cond_broadcast(&b->cond);
                pthread_mutex_unlock(&b->mtx);
                return 0;
            }
        } else {
            if (!node->read_by_storage_mgr) {
                *out = node->data;
                node->read_by_storage_mgr = 1;

                remove_fully_consumed_head(b);
                pthread_cond_broadcast(&b->cond);
                pthread_mutex_unlock(&b->mtx);
                return 0;
            }
        }

        /*
         * This consumer has already read the current head.
         * Wait until the head changes or new state appears.
         */
        pthread_cond_wait(&b->cond, &b->mtx);
    }
}

void sbuffer_close(sbuffer_t *b)
{
    if (b == NULL) {
        return;
    }

    pthread_mutex_lock(&b->mtx);
    b->closed = 1;
    pthread_cond_broadcast(&b->cond);
    pthread_mutex_unlock(&b->mtx);
}