#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

struct queue {
    uint32_t size;
    void **array;
    int in;
    int out;
    sem_t empty;
    sem_t full;
    pthread_mutex_t lock;
};

queue_t *queue_new(int size) {
    queue_t *q = calloc(1, sizeof(queue_t));
    q->size = size;
    q->array = calloc(size, sizeof(void *));
    q->in = 0;
    q->out = 0;
    pthread_mutex_init(&(q->lock), NULL);
    sem_init(&(q->empty), 0, 0);
    sem_init(&(q->full), 0, size);
    return q;
}

void queue_delete(queue_t **q) {
    if (q != NULL && *q != NULL) {
        pthread_mutex_destroy(&((*q)->lock));
        sem_destroy(&((*q)->empty));
        sem_destroy(&((*q)->full));
        free((*q)->array);
        free(*q);
        *q = NULL;
        q = NULL;
    }
}

bool queue_push(queue_t *q, void *elem) {
    if (q == NULL) {
        return false;
    } else {
        sem_wait(&(q->full));
        pthread_mutex_lock(&(q->lock));
        (q->array)[q->in] = elem;
        q->in = ((q->in) + 1) % q->size;
        pthread_mutex_unlock(&(q->lock));
        sem_post(&(q->empty));
        return true;
    }
}

bool queue_pop(queue_t *q, void **elem) {
    if (q == NULL) {
        return false;
    } else {
        sem_wait(&(q->empty));
        pthread_mutex_lock(&(q->lock));
        *elem = ((q->array)[q->out]);
        q->out = ((q->out) + 1) % q->size;
        pthread_mutex_unlock(&(q->lock));
        sem_post(&(q->full));
        return true;
    }
}
